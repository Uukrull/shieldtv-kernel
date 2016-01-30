/*
 * Media device
 *
 * Copyright (C) 2010 Nokia Corporation
 *
 * Contacts: Laurent Pinchart <laurent.pinchart@ideasonboard.com>
 *	     Sakari Ailus <sakari.ailus@iki.fi>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include <linux/compat.h>
#include <linux/export.h>
#include <linux/idr.h>
#include <linux/ioctl.h>
#include <linux/media.h>
#include <linux/slab.h>
#include <linux/types.h>

#include <media/media-device.h>
#include <media/media-devnode.h>
#include <media/media-entity.h>

#ifdef CONFIG_MEDIA_CONTROLLER

/* -----------------------------------------------------------------------------
 * Userspace API
 */

static int media_device_open(struct file *filp)
{
	return 0;
}

static int media_device_close(struct file *filp)
{
	return 0;
}

static int media_device_get_info(struct media_device *dev,
				 struct media_device_info __user *__info)
{
	struct media_device_info info;

	memset(&info, 0, sizeof(info));

	strlcpy(info.driver, dev->dev->driver->name, sizeof(info.driver));
	strlcpy(info.model, dev->model, sizeof(info.model));
	strlcpy(info.serial, dev->serial, sizeof(info.serial));
	strlcpy(info.bus_info, dev->bus_info, sizeof(info.bus_info));

	info.media_version = MEDIA_API_VERSION;
	info.hw_revision = dev->hw_revision;
	info.driver_version = dev->driver_version;

	if (copy_to_user(__info, &info, sizeof(*__info)))
		return -EFAULT;
	return 0;
}

static struct media_entity *find_entity(struct media_device *mdev, u32 id)
{
	struct media_entity *entity;
	int next = id & MEDIA_ENT_ID_FLAG_NEXT;

	id &= ~MEDIA_ENT_ID_FLAG_NEXT;

	spin_lock(&mdev->lock);

	media_device_for_each_entity(entity, mdev) {
		if (((media_entity_id(entity) == id) && !next) ||
		    ((media_entity_id(entity) > id) && next)) {
			spin_unlock(&mdev->lock);
			return entity;
		}
	}

	spin_unlock(&mdev->lock);

	return NULL;
}

static long media_device_enum_entities(struct media_device *mdev,
				       struct media_entity_desc __user *uent)
{
	struct media_entity *ent;
	struct media_entity_desc u_ent;

	memset(&u_ent, 0, sizeof(u_ent));
	if (copy_from_user(&u_ent.id, &uent->id, sizeof(u_ent.id)))
		return -EFAULT;

	ent = find_entity(mdev, u_ent.id);

	if (ent == NULL)
		return -EINVAL;

	u_ent.id = media_entity_id(ent);
	if (ent->name)
		strlcpy(u_ent.name, ent->name, sizeof(u_ent.name));
	u_ent.type = ent->function;
	u_ent.revision = 0;		/* Unused */
	u_ent.flags = ent->flags;
	u_ent.group_id = 0;		/* Unused */
	u_ent.pads = ent->num_pads;
	u_ent.links = ent->num_links - ent->num_backlinks;
	memcpy(&u_ent.raw, &ent->info, sizeof(ent->info));
	if (copy_to_user(uent, &u_ent, sizeof(u_ent)))
		return -EFAULT;
	return 0;
}

static void media_device_kpad_to_upad(const struct media_pad *kpad,
				      struct media_pad_desc *upad)
{
	upad->entity = media_entity_id(kpad->entity);
	upad->index = kpad->index;
	upad->flags = kpad->flags;
}

static long __media_device_enum_links(struct media_device *mdev,
				      struct media_links_enum *links)
{
	struct media_entity *entity;

	entity = find_entity(mdev, links->entity);
	if (entity == NULL)
		return -EINVAL;

	if (links->pads) {
		unsigned int p;

		for (p = 0; p < entity->num_pads; p++) {
			struct media_pad_desc pad;

			memset(&pad, 0, sizeof(pad));
			media_device_kpad_to_upad(&entity->pads[p], &pad);
			if (copy_to_user(&links->pads[p], &pad, sizeof(pad)))
				return -EFAULT;
		}
	}

	if (links->links) {
		struct media_link *link;
		struct media_link_desc __user *ulink_desc = links->links;

		list_for_each_entry(link, &entity->links, list) {
			struct media_link_desc klink_desc;

			/* Ignore backlinks. */
			if (link->source->entity != entity)
				continue;
			memset(&klink_desc, 0, sizeof(klink_desc));
			media_device_kpad_to_upad(link->source,
						  &klink_desc.source);
			media_device_kpad_to_upad(link->sink,
						  &klink_desc.sink);
			klink_desc.flags = link->flags;
			if (copy_to_user(ulink_desc, &klink_desc,
					 sizeof(*ulink_desc)))
				return -EFAULT;
			ulink_desc++;
		}
	}

	return 0;
}

static long media_device_enum_links(struct media_device *mdev,
				    struct media_links_enum __user *ulinks)
{
	struct media_links_enum links;
	int rval;

	if (copy_from_user(&links, ulinks, sizeof(links)))
		return -EFAULT;

	rval = __media_device_enum_links(mdev, &links);
	if (rval < 0)
		return rval;

	if (copy_to_user(ulinks, &links, sizeof(*ulinks)))
		return -EFAULT;

	return 0;
}

static long media_device_setup_link(struct media_device *mdev,
				    struct media_link_desc __user *_ulink)
{
	struct media_link *link = NULL;
	struct media_link_desc ulink;
	struct media_entity *source;
	struct media_entity *sink;
	int ret;

	if (copy_from_user(&ulink, _ulink, sizeof(ulink)))
		return -EFAULT;

	/* Find the source and sink entities and link.
	 */
	source = find_entity(mdev, ulink.source.entity);
	sink = find_entity(mdev, ulink.sink.entity);

	if (source == NULL || sink == NULL)
		return -EINVAL;

	if (ulink.source.index >= source->num_pads ||
	    ulink.sink.index >= sink->num_pads)
		return -EINVAL;

	link = media_entity_find_link(&source->pads[ulink.source.index],
				      &sink->pads[ulink.sink.index]);
	if (link == NULL)
		return -EINVAL;

	/* Setup the link on both entities. */
	ret = __media_entity_setup_link(link, ulink.flags);

	if (copy_to_user(_ulink, &ulink, sizeof(ulink)))
		return -EFAULT;

	return ret;
}

static long __media_device_get_topology(struct media_device *mdev,
				      struct media_v2_topology *topo)
{
	struct media_entity *entity;
	struct media_interface *intf;
	struct media_pad *pad;
	struct media_link *link;
	struct media_v2_entity kentity, *uentity;
	struct media_v2_interface kintf, *uintf;
	struct media_v2_pad kpad, *upad;
	struct media_v2_link klink, *ulink;
	unsigned int i;
	int ret = 0;

	topo->topology_version = mdev->topology_version;

	/* Get entities and number of entities */
	i = 0;
	uentity = media_get_uptr(topo->ptr_entities);
	media_device_for_each_entity(entity, mdev) {
		i++;
		if (ret || !uentity)
			continue;

		if (i > topo->num_entities) {
			ret = -ENOSPC;
			continue;
		}

		/* Copy fields to userspace struct if not error */
		memset(&kentity, 0, sizeof(kentity));
		kentity.id = entity->graph_obj.id;
		kentity.function = entity->function;
		strncpy(kentity.name, entity->name,
			sizeof(kentity.name));

		if (copy_to_user(uentity, &kentity, sizeof(kentity)))
			ret = -EFAULT;
		uentity++;
	}
	topo->num_entities = i;

	/* Get interfaces and number of interfaces */
	i = 0;
	uintf = media_get_uptr(topo->ptr_interfaces);
	media_device_for_each_intf(intf, mdev) {
		i++;
		if (ret || !uintf)
			continue;

		if (i > topo->num_interfaces) {
			ret = -ENOSPC;
			continue;
		}

		memset(&kintf, 0, sizeof(kintf));

		/* Copy intf fields to userspace struct */
		kintf.id = intf->graph_obj.id;
		kintf.intf_type = intf->type;
		kintf.flags = intf->flags;

		if (media_type(&intf->graph_obj) == MEDIA_GRAPH_INTF_DEVNODE) {
			struct media_intf_devnode *devnode;

			devnode = intf_to_devnode(intf);

			kintf.devnode.major = devnode->major;
			kintf.devnode.minor = devnode->minor;
		}

		if (copy_to_user(uintf, &kintf, sizeof(kintf)))
			ret = -EFAULT;
		uintf++;
	}
	topo->num_interfaces = i;

	/* Get pads and number of pads */
	i = 0;
	upad = media_get_uptr(topo->ptr_pads);
	media_device_for_each_pad(pad, mdev) {
		i++;
		if (ret || !upad)
			continue;

		if (i > topo->num_pads) {
			ret = -ENOSPC;
			continue;
		}

		memset(&kpad, 0, sizeof(kpad));

		/* Copy pad fields to userspace struct */
		kpad.id = pad->graph_obj.id;
		kpad.entity_id = pad->entity->graph_obj.id;
		kpad.flags = pad->flags;

		if (copy_to_user(upad, &kpad, sizeof(kpad)))
			ret = -EFAULT;
		upad++;
	}
	topo->num_pads = i;

	/* Get links and number of links */
	i = 0;
	ulink = media_get_uptr(topo->ptr_links);
	media_device_for_each_link(link, mdev) {
		if (link->is_backlink)
			continue;

		i++;

		if (ret || !ulink)
			continue;

		if (i > topo->num_links) {
			ret = -ENOSPC;
			continue;
		}

		memset(&klink, 0, sizeof(klink));

		/* Copy link fields to userspace struct */
		klink.id = link->graph_obj.id;
		klink.source_id = link->gobj0->id;
		klink.sink_id = link->gobj1->id;
		klink.flags = link->flags;

		if (copy_to_user(ulink, &klink, sizeof(klink)))
			ret = -EFAULT;
		ulink++;
	}
	topo->num_links = i;

	return ret;
}

static long media_device_get_topology(struct media_device *mdev,
				      struct media_v2_topology __user *utopo)
{
	struct media_v2_topology ktopo;
	int ret;

	if (copy_from_user(&ktopo, utopo, sizeof(ktopo)))
		return -EFAULT;

	ret = __media_device_get_topology(mdev, &ktopo);
	if (ret < 0)
		return ret;

	if (copy_to_user(utopo, &ktopo, sizeof(*utopo)))
		return -EFAULT;

	return 0;
}

static long media_device_ioctl(struct file *filp, unsigned int cmd,
			       unsigned long arg)
{
	struct media_devnode *devnode = media_devnode_data(filp);
	struct media_device *dev = to_media_device(devnode);
	long ret;

	switch (cmd) {
	case MEDIA_IOC_DEVICE_INFO:
		ret = media_device_get_info(dev,
				(struct media_device_info __user *)arg);
		break;

	case MEDIA_IOC_ENUM_ENTITIES:
		ret = media_device_enum_entities(dev,
				(struct media_entity_desc __user *)arg);
		break;

	case MEDIA_IOC_ENUM_LINKS:
		mutex_lock(&dev->graph_mutex);
		ret = media_device_enum_links(dev,
				(struct media_links_enum __user *)arg);
		mutex_unlock(&dev->graph_mutex);
		break;

	case MEDIA_IOC_SETUP_LINK:
		mutex_lock(&dev->graph_mutex);
		ret = media_device_setup_link(dev,
				(struct media_link_desc __user *)arg);
		mutex_unlock(&dev->graph_mutex);
		break;

	case MEDIA_IOC_G_TOPOLOGY:
		mutex_lock(&dev->graph_mutex);
		ret = media_device_get_topology(dev,
				(struct media_v2_topology __user *)arg);
		mutex_unlock(&dev->graph_mutex);
		break;

	default:
		ret = -ENOIOCTLCMD;
	}

	return ret;
}

#ifdef CONFIG_COMPAT

struct media_links_enum32 {
	__u32 entity;
	compat_uptr_t pads; /* struct media_pad_desc * */
	compat_uptr_t links; /* struct media_link_desc * */
	__u32 reserved[4];
};

static long media_device_enum_links32(struct media_device *mdev,
				      struct media_links_enum32 __user *ulinks)
{
	struct media_links_enum links;
	compat_uptr_t pads_ptr, links_ptr;

	memset(&links, 0, sizeof(links));

	if (get_user(links.entity, &ulinks->entity)
	    || get_user(pads_ptr, &ulinks->pads)
	    || get_user(links_ptr, &ulinks->links))
		return -EFAULT;

	links.pads = compat_ptr(pads_ptr);
	links.links = compat_ptr(links_ptr);

	return __media_device_enum_links(mdev, &links);
}

#define MEDIA_IOC_ENUM_LINKS32		_IOWR('|', 0x02, struct media_links_enum32)

static long media_device_compat_ioctl(struct file *filp, unsigned int cmd,
				      unsigned long arg)
{
	struct media_devnode *devnode = media_devnode_data(filp);
	struct media_device *dev = to_media_device(devnode);
	long ret;

	switch (cmd) {
	case MEDIA_IOC_DEVICE_INFO:
	case MEDIA_IOC_ENUM_ENTITIES:
	case MEDIA_IOC_SETUP_LINK:
	case MEDIA_IOC_G_TOPOLOGY:
		return media_device_ioctl(filp, cmd, arg);

	case MEDIA_IOC_ENUM_LINKS32:
		mutex_lock(&dev->graph_mutex);
		ret = media_device_enum_links32(dev,
				(struct media_links_enum32 __user *)arg);
		mutex_unlock(&dev->graph_mutex);
		break;

	default:
		ret = -ENOIOCTLCMD;
	}

	return ret;
}
#endif /* CONFIG_COMPAT */

static const struct media_file_operations media_device_fops = {
	.owner = THIS_MODULE,
	.open = media_device_open,
	.ioctl = media_device_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl = media_device_compat_ioctl,
#endif /* CONFIG_COMPAT */
	.release = media_device_close,
};

/* -----------------------------------------------------------------------------
 * sysfs
 */

static ssize_t show_model(struct device *cd,
			  struct device_attribute *attr, char *buf)
{
	struct media_device *mdev = to_media_device(to_media_devnode(cd));

	return sprintf(buf, "%.*s\n", (int)sizeof(mdev->model), mdev->model);
}

static DEVICE_ATTR(model, S_IRUGO, show_model, NULL);

/* -----------------------------------------------------------------------------
 * Registration/unregistration
 */

static void media_device_release(struct media_devnode *mdev)
{
	dev_dbg(mdev->parent, "Media device released\n");
}

/**
 * media_device_register_entity - Register an entity with a media device
 * @mdev:	The media device
 * @entity:	The entity
 */
int __must_check media_device_register_entity(struct media_device *mdev,
					      struct media_entity *entity)
{
	unsigned int i;
	int ret;

	if (entity->function == MEDIA_ENT_F_V4L2_SUBDEV_UNKNOWN ||
	    entity->function == MEDIA_ENT_F_UNKNOWN)
		dev_warn(mdev->dev,
			 "Entity type for entity %s was not initialized!\n",
			 entity->name);

	/* Warn if we apparently re-register an entity */
	WARN_ON(entity->graph_obj.mdev != NULL);
	entity->graph_obj.mdev = mdev;
	INIT_LIST_HEAD(&entity->links);
	entity->num_links = 0;
	entity->num_backlinks = 0;

	if (!ida_pre_get(&mdev->entity_internal_idx, GFP_KERNEL))
		return -ENOMEM;

	spin_lock(&mdev->lock);

	ret = ida_get_new_above(&mdev->entity_internal_idx, 1,
				&entity->internal_idx);
	if (ret < 0) {
		spin_unlock(&mdev->lock);
		return ret;
	}

	mdev->entity_internal_idx_max =
		max(mdev->entity_internal_idx_max, entity->internal_idx);

	/* Initialize media_gobj embedded at the entity */
	media_gobj_create(mdev, MEDIA_GRAPH_ENTITY, &entity->graph_obj);

	/* Initialize objects at the pads */
	for (i = 0; i < entity->num_pads; i++)
		media_gobj_create(mdev, MEDIA_GRAPH_PAD,
			       &entity->pads[i].graph_obj);

	spin_unlock(&mdev->lock);

	return 0;
}
EXPORT_SYMBOL_GPL(media_device_register_entity);

static void __media_device_unregister_entity(struct media_entity *entity)
{
	struct media_device *mdev = entity->graph_obj.mdev;
	struct media_link *link, *tmp;
	struct media_interface *intf;
	unsigned int i;

	ida_simple_remove(&mdev->entity_internal_idx, entity->internal_idx);

	/* Remove all interface links pointing to this entity */
	list_for_each_entry(intf, &mdev->interfaces, graph_obj.list) {
		list_for_each_entry_safe(link, tmp, &intf->links, list) {
			if (link->entity == entity)
				__media_remove_intf_link(link);
		}
	}

	/* Remove all data links that belong to this entity */
	__media_entity_remove_links(entity);

	/* Remove all pads that belong to this entity */
	for (i = 0; i < entity->num_pads; i++)
		media_gobj_destroy(&entity->pads[i].graph_obj);

	/* Remove the entity */
	media_gobj_destroy(&entity->graph_obj);

	entity->graph_obj.mdev = NULL;
}

void media_device_unregister_entity(struct media_entity *entity)
{
	struct media_device *mdev = entity->graph_obj.mdev;

	if (mdev == NULL)
		return;

	spin_lock(&mdev->lock);
	__media_device_unregister_entity(entity);
	spin_unlock(&mdev->lock);
}
EXPORT_SYMBOL_GPL(media_device_unregister_entity);

/**
 * media_device_init() - initialize a media device
 * @mdev:	The media device
 *
 * The caller is responsible for initializing the media device before
 * registration. The following fields must be set:
 *
 * - dev must point to the parent device
 * - model must be filled with the device model name
 */
void media_device_init(struct media_device *mdev)
{
	INIT_LIST_HEAD(&mdev->entities);
	INIT_LIST_HEAD(&mdev->interfaces);
	INIT_LIST_HEAD(&mdev->pads);
	INIT_LIST_HEAD(&mdev->links);
	spin_lock_init(&mdev->lock);
	mutex_init(&mdev->graph_mutex);
	ida_init(&mdev->entity_internal_idx);

	dev_dbg(mdev->dev, "Media device initialized\n");
}
EXPORT_SYMBOL_GPL(media_device_init);

void media_device_cleanup(struct media_device *mdev)
{
	ida_destroy(&mdev->entity_internal_idx);
	mdev->entity_internal_idx_max = 0;
	mutex_destroy(&mdev->graph_mutex);
}
EXPORT_SYMBOL_GPL(media_device_cleanup);

int __must_check __media_device_register(struct media_device *mdev,
					 struct module *owner)
{
	int ret;

	/* Register the device node. */
	mdev->devnode.fops = &media_device_fops;
	mdev->devnode.parent = mdev->dev;
	mdev->devnode.release = media_device_release;

	/* Set version 0 to indicate user-space that the graph is static */
	mdev->topology_version = 0;

	ret = media_devnode_register(&mdev->devnode, owner);
	if (ret < 0)
		return ret;

	ret = device_create_file(&mdev->devnode.dev, &dev_attr_model);
	if (ret < 0) {
		media_devnode_unregister(&mdev->devnode);
		return ret;
	}

	dev_dbg(mdev->dev, "Media device registered\n");

	return 0;
}
EXPORT_SYMBOL_GPL(__media_device_register);

void media_device_unregister(struct media_device *mdev)
{
	struct media_entity *entity;
	struct media_entity *next;
	struct media_interface *intf, *tmp_intf;

	if (mdev == NULL)
		return;

	spin_lock(&mdev->lock);

	/* Check if mdev was ever registered at all */
	if (!media_devnode_is_registered(&mdev->devnode)) {
		spin_unlock(&mdev->lock);
		return;
	}

	/* Remove all entities from the media device */
	list_for_each_entry_safe(entity, next, &mdev->entities, graph_obj.list)
		__media_device_unregister_entity(entity);

	/* Remove all interfaces from the media device */
	list_for_each_entry_safe(intf, tmp_intf, &mdev->interfaces,
				 graph_obj.list) {
		__media_remove_intf_links(intf);
		media_gobj_destroy(&intf->graph_obj);
		kfree(intf);
	}

	spin_unlock(&mdev->lock);

	device_remove_file(&mdev->devnode.dev, &dev_attr_model);
	media_devnode_unregister(&mdev->devnode);

	dev_dbg(mdev->dev, "Media device unregistered\n");
}
EXPORT_SYMBOL_GPL(media_device_unregister);

static void media_device_release_devres(struct device *dev, void *res)
{
}

struct media_device *media_device_get_devres(struct device *dev)
{
	struct media_device *mdev;

	mdev = devres_find(dev, media_device_release_devres, NULL, NULL);
	if (mdev)
		return mdev;

	mdev = devres_alloc(media_device_release_devres,
				sizeof(struct media_device), GFP_KERNEL);
	if (!mdev)
		return NULL;
	return devres_get(dev, mdev, NULL, NULL);
}
EXPORT_SYMBOL_GPL(media_device_get_devres);

struct media_device *media_device_find_devres(struct device *dev)
{
	return devres_find(dev, media_device_release_devres, NULL, NULL);
}
EXPORT_SYMBOL_GPL(media_device_find_devres);

#endif /* CONFIG_MEDIA_CONTROLLER */
