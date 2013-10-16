#include <linux/module.h>
#include <linux/slab.h>

#include <linux/buffer_head.h>

#include "thiagofs.h"

extern unsigned long inode_number;

static void thiagofs_sync_sb(struct super_block * sb)
{
  struct buffer_head * bh;
  struct inode * inode;
  struct thiagofs_inode * ti;
  struct thiagofs_super_block *tsb;
  struct file_contents *f;

  if (!(bh = sb_bread(sb, THIAGOFS_ROOT_INODE))) {
    printk("thiagofs: error: failed to read superblock.\n");
    return;
  }

  tsb = (struct thiagofs_super_block *) ((char *)bh->b_data);
  tsb->s_max_inode_number_in_use = inode_number;

  ti = &(tsb->root_inode);
  
  inode = thiagofs_find_inode_by_ino(THIAGOFS_ROOT_INODE);

  ti->i_ino = inode->i_ino;
  ti->i_mtime = inode->i_mtime.tv_sec;
  ti->i_atime = inode->i_atime.tv_sec;
  ti->i_ctime = inode->i_ctime.tv_sec;
  ti->i_mode = inode->i_mode;
  ti->i_blocks = inode->i_blocks;
  if (inode->i_size > THIAGOFS_MAX_FILESIZE)
    ti->i_size = THIAGOFS_MAX_FILESIZE;
  else
    ti->i_size = inode->i_size;
 
  f = thiagofs_find_file(inode);
  memcpy(&(ti->data), f->conts, ti->i_size);
  mark_buffer_dirty_inode(bh, inode);
  mark_inode_dirty(inode);
  sync_dirty_buffer(bh);
 
  return;
}

int thiagofs_write_inode(struct inode * inode, struct writeback_control *wbc) {
  struct buffer_head * bh;
  struct thiagofs_inode * ti;
  struct file_contents *f;
  struct super_block * sb = inode->i_sb;

  if (inode->i_ino == THIAGOFS_ROOT_INODE) {
    thiagofs_sync_sb(sb);
    return 0;
  }
  /* Node writeback */
  if (!(bh = sb_bread(sb, inode->i_ino))) {
    printk("thiagofs: error: failed to read block %lu.\n",inode->i_ino);
    return -EINVAL;
  }
  ti = (struct thiagofs_inode *) ((char *)bh->b_data);
  
  ti->i_ino = inode->i_ino;
  ti->i_mtime = inode->i_mtime.tv_sec;
  ti->i_atime = inode->i_atime.tv_sec;
  ti->i_ctime = inode->i_ctime.tv_sec;
  ti->i_blocks = inode->i_blocks;
  ti->i_uid = inode->i_uid;
  ti->i_gid = inode->i_gid;
  ti->i_mode = inode->i_mode;
  if (inode->i_size > THIAGOFS_MAX_FILESIZE)
    ti->i_size = THIAGOFS_MAX_FILESIZE;
  else
    ti->i_size = inode->i_size;
  
  f = thiagofs_find_file(inode);
  if (f == NULL) return -EIO;
  
  memcpy(&(ti->data), f->conts, ti->i_size);
  mark_buffer_dirty_inode(bh, inode); 
  mark_inode_dirty(inode);
  sync_dirty_buffer(bh);

  return 0;
}

static int thiagofs_drop_inode(struct inode * inode)
{
  struct file_contents *f;
  f = thiagofs_find_file(inode);
  if(inode->i_private != NULL) kfree(inode->i_private);
  if(f != NULL) kfree(f);

  return generic_drop_inode(inode);
}

static const struct super_operations thiagofs_ops = {
  .statfs         = simple_statfs,
  .drop_inode     = thiagofs_drop_inode,
  .write_inode    = thiagofs_write_inode,
};



static int thiagofs_fill_super(struct super_block *sb, void *data, int silent)
{
  struct buffer_head * bh;
  struct inode * inode;
  struct thiagofs_inode * ti;
  struct dentry * root;
  struct thiagofs_super_block *tsb;

  struct file_contents *file = kzalloc(sizeof(*file), GFP_KERNEL);

  if (!file)
    return -EAGAIN;

  printk("thiagofs: info: dev %s, bsize %lu.\n", sb->s_id, sb->s_blocksize);

  if (!(bh = sb_bread(sb, THIAGOFS_ROOT_INODE))) {
    printk("thiagofs: error: failed to read superblock.\n");
    return -EIO;
  }
  
  tsb = (struct thiagofs_super_block *) ((char *)bh->b_data);
  if (tsb)
    return -EINVAL;

  if (tsb->s_magic != THIAGOFS_SUPER_MAGIC){
    printk("thiagofs: error: magic number doesn't match. Value read: %u.\n", tsb->s_magic);
    brelse(bh);
    return -EINVAL;
  }
  inode_number = tsb->s_max_inode_number_in_use;

  sb->s_maxbytes = THIAGOFS_MAX_FILESIZE;
  sb->s_blocksize = 4096;
  sb->s_blocksize_bits = 12;
  
  sb->s_op = &thiagofs_ops;
  sb->s_time_gran = 1;

  ti = &(tsb->root_inode);
  
  inode = new_inode(sb);

  if (!inode)
    return -ENOMEM;

  inode->i_ino = ti->i_ino;
  inode->i_mtime.tv_sec = ti->i_mtime;
  inode->i_atime.tv_sec = ti->i_atime;
  inode->i_ctime.tv_sec = ti->i_ctime;
  inode->i_atime.tv_nsec = inode->i_mtime.tv_nsec = inode->i_ctime.tv_nsec = 0;
  inode->i_blocks = ti->i_blocks;
  inode->i_uid = ti->i_uid;
  inode->i_gid = ti->i_gid;
  inode->i_mode = S_IFDIR | S_IRUGO | S_IXUGO | S_IWUSR;
  inode->i_op = &thiagofs_dir_inode_operations;
  inode->i_fop = &thiagofs_file_operations;
  inode->i_size = 0;
  set_nlink(inode, 2);

  ti = kzalloc(sizeof(*ti), GFP_KERNEL);
  file->inode = inode;
  inode->i_private = (void *)ti;

  file->conts = (void *)ti->data;
  INIT_LIST_HEAD(&file->list);
  list_add_tail(&file->list, &contents_list); 
  root = d_make_root(inode);
  if (!root) {
    iput(inode);
    return -ENOMEM;
  }
  sb->s_root = root;

  /*! Criação do arquivo de journal !*/
  thiagofs_create_journal(sb, root);

  brelse(bh);
  return 0;

}

static void thiagofs_kill_block_super(struct super_block * sb) {
  //struct file_contents *f;
  thiagofs_sync_sb(sb);
  //list_for_each_entry(f, &contents_list, list) {
    //if (atomic_read(f->inode->i_count) > 0) iput(f->inode);
  //}
  d_genocide(sb->s_root);
  shrink_dcache_parent(sb->s_root);
  kill_block_super(sb);
}

static struct dentry *thiagofs_get_sb(struct file_system_type *fs_type, int flags, const char *dev_name, void *data)
{
  return mount_bdev(fs_type, flags, dev_name, data, thiagofs_fill_super);
}

static struct file_system_type thiagofs_fs_type = {
  .owner    = THIS_MODULE,
  .name     = "thiagofs",
  .mount    = thiagofs_get_sb,
  .kill_sb  = thiagofs_kill_block_super,
};

static int __init init_thiagofs_fs(void)
{
  INIT_LIST_HEAD(&contents_list);
  return register_filesystem(&thiagofs_fs_type);
}

static void __exit exit_thiagofs_fs(void)
{
  unregister_filesystem(&thiagofs_fs_type);
}

module_init(init_thiagofs_fs)
module_exit(exit_thiagofs_fs)
MODULE_AUTHOR("Thiago Santiago");
MODULE_LICENSE("GPL");
