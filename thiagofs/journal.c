#include <linux/types.h>
#include <linux/fs.h>
#include <linux/slab.h>
#include <linux/mm.h>

#include <linux/buffer_head.h>

#include "thiagofs.h"

struct inode * journal_inode;
extern unsigned long inode_number;

/* Função para escrever no arquivo de journal.
 * Ainda está muito crua. Escreve na area de dados da struct thiagofs_inode. Tem que escrever numa página de memória. */
int thiagofs_write_journal(char * buf)
{
  struct file_contents *f;

  f = thiagofs_find_file(journal_inode);
  if (f == NULL)
    return -EIO;  

  f->conts = (void*) strcat((char *)f->conts,buf);

  f->inode->i_size = strlen((char *)(f->conts));
 
  thiagofs_sync_inode(f->inode);  

  return 0;
}

int thiagofs_create_journal(struct super_block *sb, struct dentry *root)
{
  struct inode * inode;
  struct dentry * dentry;
  struct buffer_head * bh;
  struct thiagofs_inode * ti = NULL;
  struct file_contents *file = kzalloc(sizeof(*file), GFP_KERNEL);  
  char journal_log[30];

  if (!file)
    return -EAGAIN;

  dentry = d_alloc_name(root, ".journal");
  if (!dentry)
    goto out;
  inode = new_inode(sb);
  if (!inode) {
    dput(dentry);
    goto out;
  }
  if(inode_number<THIAGOFS_JOURNAL_INODE) { 
    inode->i_mode = S_IFREG | S_IRUGO;
    inode->i_atime = inode->i_mtime = inode->i_ctime = CURRENT_TIME;
    inode->i_ino = ++inode_number;
    inode->i_size = 0;
  }
  else {
    if (!(bh = sb_bread(sb, THIAGOFS_JOURNAL_INODE))) {
      printk("thiagofs: error: failed to read journal block.\n");
      return -EINVAL;
    }
    ti = (struct thiagofs_inode *) ((char *)bh->b_data);
    inode->i_ino = ti->i_ino;
    inode->i_mtime.tv_sec = ti->i_mtime;
    inode->i_atime.tv_sec = ti->i_atime;
    inode->i_ctime.tv_sec = ti->i_ctime;
    inode->i_atime.tv_nsec = inode->i_mtime.tv_nsec = inode->i_ctime.tv_nsec = 0;
    inode->i_blocks = ti->i_blocks;
    inode->i_uid = ti->i_uid;
    inode->i_gid = ti->i_gid;
    inode->i_mode = ti->i_mode;
    inode->i_size = ti->i_size;
  }
  journal_inode = inode;
  inode->i_op = &thiagofs_file_inode_operations;
  inode->i_fop = &thiagofs_file_operations;

  file->inode = inode;
  inode->i_private = (void *)ti;

  INIT_LIST_HEAD(&file->list);
  list_add_tail(&file->list, &contents_list); 
  
  if(ti && inode->i_size) { 
    file->conts = (void *)ti->data;
  } 
  else {
    ti = kzalloc(sizeof(*ti), GFP_KERNEL);
    file->conts = (void *)ti->data;
    sprintf(journal_log, "====== Journal created ======\n");
    thiagofs_write_journal(journal_log);
  }
  
  thiagofs_dir_add_link(root->d_inode, inode, dentry->d_name.name, dentry->d_name.len);
  d_add(dentry, inode);

  return 0;
out:
  d_genocide(root);
  shrink_dcache_parent(root);
  dput(root);
  return -ENOMEM;
}

