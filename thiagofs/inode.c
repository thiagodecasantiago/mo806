#include <linux/types.h>
#include <linux/fs.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/mm.h>
#include <linux/buffer_head.h>

#include <asm/current.h>

#include "thiagofs.h"

unsigned long inode_number = 0;

static const struct inode_operations thiagofs_file_inode_operations = {
  .getattr        = simple_getattr,
};


/* Lembram quando eu disse que um hash seria mais eficiente? ;-) */
struct file_contents *thiagofs_find_file(struct inode *inode)
{
  struct file_contents *f;
  list_for_each_entry(f, &contents_list, list) {
    if (f->inode == inode)
      return f;
  }
  return NULL;
}

struct inode *thiagofs_find_inode_by_ino(unsigned long ino)
{
  struct file_contents *f;
  list_for_each_entry(f, &contents_list, list) {
    if (f->inode->i_ino == ino)
      return f->inode;
  }
  return NULL;
}

int thiagofs_sync_inode(struct inode * inode)
{
  return thiagofs_write_inode(inode, NULL);
}

static struct inode *thiagofs_new_inode(struct super_block *sb, umode_t mode)
{
  struct inode *inode;
  struct file_contents *file = kzalloc(sizeof(*file), GFP_KERNEL);
  struct thiagofs_inode *ti = kzalloc(sizeof(*ti), GFP_KERNEL);
  
  if (!file)
    return NULL;

  inode = new_inode(sb);
  if (inode) {
    inode->i_mode = mode;
    inode->i_uid = current->cred->fsuid;
    inode->i_gid = current->cred->fsgid;
    inode->i_blocks = 0;
    inode->i_atime = inode->i_mtime = inode->i_ctime = CURRENT_TIME;
    inode->i_ino = ++inode_number;
    switch (mode & S_IFMT) {
      case S_IFREG:
        inode->i_op = &thiagofs_file_inode_operations;
        inode->i_fop = &thiagofs_file_operations;
        break;
      case S_IFDIR:
        inode->i_op = &thiagofs_dir_inode_operations;
        inode->i_fop = &thiagofs_file_operations;
        break;
    }
  }
  
  file->inode = inode;
  inode->i_private = (void *)ti;

  file->conts = (void *)ti->data;
  INIT_LIST_HEAD(&file->list);
  list_add_tail(&file->list, &contents_list); 
  
  /* Write new inode into the disk */
  thiagofs_sync_inode(inode);

  return inode;
}

int thiagofs_dir_add_link(struct inode *parent_dir, struct inode *child, const char *name, int namelen)
{
  int err = 0;
  struct thiagofs_directory_record *last_dir_rec = NULL;
  struct file_contents *fc;

  /* sanity checks. */
  if (namelen > THIAGOFS_MAX_NAME_SIZE) {
    err = -ENAMETOOLONG;
    goto ret_err;
  }

  /* read the data of the parent directory. */
  fc = thiagofs_find_file(parent_dir);

  /* find the last entry in the parent directory. */
  last_dir_rec = (struct thiagofs_directory_record *)(fc->conts);
  for ( ; ((char*)last_dir_rec) < ((char*)fc->conts) + THIAGOFS_MAX_FILESIZE; last_dir_rec++) {
    if (last_dir_rec->dr_inode == 0)
      break; /* last entry found. */
  }

  /* if no free entry found... */
  if (((char*)last_dir_rec) >= ((char*)fc->conts) + THIAGOFS_MAX_FILESIZE) {
    err = -ENOSPC;
    goto ret_err;
  }

  /* ok, populate the entry. */
  last_dir_rec->dr_inode = child->i_ino;
  if (S_ISDIR(child->i_mode))
    last_dir_rec->dr_ftype = 1;
  else
    last_dir_rec->dr_ftype = 0;
  strncpy(last_dir_rec->dr_name, name, namelen);
  parent_dir->i_mtime = parent_dir->i_ctime = CURRENT_TIME;
  parent_dir->i_size += sizeof(struct thiagofs_directory_record);
  thiagofs_sync_inode(parent_dir);

  /* all went well... */
  err = 0;
  goto ret;

ret_err:
        /* fallthrough */
ret:
  return err;
}

static int thiagofs_mkdir (struct inode *parent, struct dentry *dentry, umode_t mode)
{
  int err;
  struct inode *child_dir = NULL;

  /* increase the link count of the parent dir (the child dir will have
   * a '..' entry pointing back to the parent dir). */
  inc_nlink(parent);
  mark_inode_dirty(parent);

  /*-- Creating node  --*/
  child_dir = thiagofs_new_inode(parent->i_sb, mode | S_IFDIR);

  d_instantiate(dentry, child_dir);  
  dget(dentry);

  /* link the child as an inode in the parent. */
  err = thiagofs_dir_add_link(parent, child_dir, dentry->d_name.name, dentry->d_name.len);
  if (err)
    goto ret;

  /* finally, instantiate the child's dentry. */
  d_instantiate(dentry, child_dir);

  thiagofs_sync_inode(child_dir);

  err = 0;
  goto ret;

  if (child_dir)
    iput(child_dir); /* child_dir will be deleted here. */
ret:
  return err;

}


/* criacao de um arquivo: sem misterio, sem segredo, apenas
 * alocar as estruturas, preencher, e retornar */
static int thiagofs_create (struct inode *dir, struct dentry * dentry, umode_t mode, bool excl)
{
  
  struct inode *inode;

  char journal_log[50];
  sprintf(journal_log, "Creating file %s...\n", dentry->d_name.name);
  thiagofs_write_journal(journal_log);

  inode = thiagofs_new_inode(dir->i_sb, mode | S_IFREG);

  thiagofs_dir_add_link(dir, inode, dentry->d_name.name, dentry->d_name.len);
  
  d_instantiate(dentry, inode);  
  dget(dentry);

  sprintf(journal_log, "File %s created.\n", dentry->d_name.name);
  thiagofs_write_journal(journal_log);

  return 0;
}

static int thiagofs_link(struct dentry *old_dentry, struct inode *dir, struct dentry *dentry)
{
  int ret;
  char journal_log[150];
  sprintf(journal_log, "Creating %s (copy of %s)...\n", dentry->d_name.name, old_dentry->d_name.name);
  thiagofs_write_journal(journal_log);
  
  ret = simple_link(old_dentry, dir, dentry);

  if (!ret) {
    sprintf(journal_log, "%s(copy of %s) created.\n", dentry->d_name.name, old_dentry->d_name.name);
    thiagofs_write_journal(journal_log);  
  }

  return ret;
}

static int thiagofs_unlink(struct inode *dir, struct dentry *dentry)
{
  int ret;
  char journal_log[150];
  sprintf(journal_log, "Deleting %s ...\n", dentry->d_name.name);
  thiagofs_write_journal(journal_log);
  
  ret = simple_unlink(dir, dentry);

  if (!ret) {
    sprintf(journal_log, "%s deleted. Remaining links to inode <%lu>: %u.\n", dentry->d_name.name, dentry->d_inode->i_ino, dentry->d_inode->i_nlink);
    thiagofs_write_journal(journal_log);  
  }

  return ret;
}

static int thiagofs_rename(struct inode *old_dir, struct dentry *old_dentry, struct inode *new_dir, struct dentry *new_dentry)
{
  int ret;
  char journal_log[150];
  sprintf(journal_log, "Moving %s to %s.\n", old_dentry->d_name.name, new_dentry->d_name.name);
  thiagofs_write_journal(journal_log);
  
  ret = simple_rename(old_dir, old_dentry, new_dir, new_dentry);

  if (!ret) {
    sprintf(journal_log, "%s moved to %s.\n", old_dentry->d_name.name, new_dentry->d_name.name);
    thiagofs_write_journal(journal_log);  
  }

  return ret;
}


static int thiagofs_rmdir(struct inode *parent_dir, struct dentry *dentry)
{
  int err = 0;
  struct inode *child_dir = dentry->d_inode;

  err = thiagofs_unlink(parent_dir, dentry);
  if (err)
    goto ret_err;

  child_dir->i_size = 0;

  /* the parent no longer points to the child, and the child's '..'
   * entry no longer points to the parent. */
  drop_nlink(parent_dir);
  thiagofs_sync_inode(parent_dir);
  drop_nlink(child_dir);
  thiagofs_sync_inode(child_dir);

  /* all went well... */
  err = 0;
  goto ret;

ret_err:
  /* fall through... */
ret:
  return err;
}

int thiagofs_readdir(struct file *filp, void *dirent, filldir_t filldir)
{
  struct dentry *dentry = filp->f_dentry;
  struct inode *dir = filp->f_dentry->d_inode;
  struct super_block* sb = dir->i_sb;
  struct buffer_head *bh = NULL;
  struct thiagofs_directory_record *dir_rec;
  struct thiagofs_super_block * tsb;
  struct thiagofs_inode * ti;
  int err = 0;
  int over;
  int teste=0;

  /* we have no problem with an empty dir (which should contain '.'
   * and '..', since we always allocate one block for the dir's data. */
  if (filp->f_pos -2> dir->i_size /*- 32*/) {
    printk("thiagofs: error: file pos larger than dir size.\n");
    goto done;
  }

  /* special handling for '.' and '..' */
  if (filp->f_pos == 0) {
    over = filldir(dirent, ".", 1, filp->f_pos,
        dir->i_ino, DT_DIR);
    if (over < 0)
      goto done;
    filp->f_pos++;
  }
  if (filp->f_pos == 1) {
    over = filldir(dirent, "..", 2, filp->f_pos,
        dentry->d_parent->d_inode->i_ino, DT_DIR);
    if (over < 0)
      goto done;
    filp->f_pos++;
  }

  /* read in the data block of this directory. */
  if (!(bh = sb_bread(sb, dir->i_ino))) {
    printk("thiagofs: error: failed to read block %lu.\n", dir->i_ino);
    err = -EIO;
    goto done;
  }

  /* loop over the dir entries, until we finish scanning, or until
   * we finish filling the dirent's available space */
  /* note: the '- 2' is because of the phony "." and ".." entries. */
  if (dir->i_ino == THIAGOFS_ROOT_INODE){   
    tsb = (struct thiagofs_super_block *) ((char *)bh->b_data);
    ti = &(tsb->root_inode);
  }
  else {
    ti = (struct thiagofs_inode *) ((char *)bh->b_data);
  }
  dir_rec = (struct thiagofs_directory_record *)(ti->data);
  dir_rec += (filp->f_pos -2)/32;
  while (1) {
    unsigned char d_type = DT_UNKNOWN;
    /* dr_ino == 0 implies end-of-list. */
    if (dir_rec->dr_inode == 0)
      break;

    /* add this entry, unless it's marked as 'free'. */
    if (dir_rec->dr_ftype == 1)
      d_type = DT_DIR;
    if (dir_rec->dr_ftype == 0)
      d_type = DT_REG;

    over = filldir(dirent, dir_rec->dr_name, THIAGOFS_MAX_NAME_SIZE, filp->f_pos, dir_rec->dr_inode, d_type);
    if(over < 0)
      goto done;

    /* skip to the next entry. */
    filp->f_pos += 32/*sizeof(struct thiagofs_directory_record)*/;

    /* note: again, the '+ 2' is because of the
     * phony "." and ".." entries. */
    if (filp->f_pos >= THIAGOFS_MAX_FILESIZE + 2)
      goto done;
    dir_rec++;
    teste++;
  }

done:
  dir->i_atime = CURRENT_TIME;

  if (bh)
    brelse(bh);

  return err;
}

static const struct inode_operations thiagofs_dir_inode_operations = {
  .create = thiagofs_create,
  .lookup = simple_lookup,
  .link   = thiagofs_link,
  .unlink = thiagofs_unlink,
  .rename = thiagofs_rename,
  .mkdir  = thiagofs_mkdir,
  .rmdir  = thiagofs_rmdir,
};


