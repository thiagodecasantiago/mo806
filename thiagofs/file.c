#include <linux/types.h>
#include <linux/fs.h>

#include <asm/uaccess.h>

#include <linux/buffer_head.h>

#include "thiagofs.h"

/* Apos passar pelo VFS, uma leitura chegara aqui. A unica
 * coisa que fazemos eh, achar o ponteiro para o conteudo do arquivo,
 * e retornar, de acordo com o tamanho solicitado */
static ssize_t thiagofs_read(struct file *file, char __user *buf,
          size_t count, loff_t *pos)
{
  struct file_contents *f;
  struct inode *inode = file->f_path.dentry->d_inode;
  int size = count;

  f = thiagofs_find_file(inode);
  if (f == NULL)
    return -EIO;
    
  if (f->inode->i_size < count)
    size = f->inode->i_size;

  if ((*pos + size) >= f->inode->i_size)
    size = f->inode->i_size - *pos;
  
  /* As page tables do kernel estao sempre mapeadas (veremos o que
   * sao page tables mais pra frente do curso), mas o mesmo nao eh
   * verdade com as paginas de espaco de usuario. Dessa forma, uma
   * atribuicao de/para um ponteiro contendo um endedereco de espaco
   * de usuario pode falhar. Dessa forma, toda a comunicacao
   * de/para espaco de usuario eh feita com as funcoes copy_from_user()
   * e copy_to_user(). */
  if (copy_to_user(buf, f->conts + *pos, size))
    return -EFAULT;
  *pos += size;

  return size;
}

/* similar a leitura, mas vamos escrever no ponteiro do conteudo.
 * Por simplicidade, estamos escrevendo sempre no comeco do arquivo.
 * Obviamente, esse nao eh o comportamento esperado de um write 'normal'
 * Mas implementacoes de sistemas de arquivos sao flexiveis... */
static ssize_t thiagofs_write(struct file *file, const char __user *buf,
           size_t count, loff_t *pos)
{
  struct file_contents *f;
  struct inode *inode = file->f_path.dentry->d_inode;
  
  char journal_log[100];
  sprintf(journal_log, "Archive %s: writting %d bytes.\n", file->f_path.dentry->d_name.name, count);
  thiagofs_write_journal(journal_log);

  f = thiagofs_find_file(inode);
  if (f == NULL)
    return -ENOENT;
    
  /* copy_from_user() : veja comentario na funcao de leitura */
  if (copy_from_user(f->conts + *pos, buf, count))
    return -EFAULT;

  inode->i_size = count;

  sprintf(journal_log, "Archive %s: %d bytes written.\n", file->f_path.dentry->d_name.name, count);
  thiagofs_write_journal(journal_log);

  mark_inode_dirty(inode);
  thiagofs_sync_inode(inode);

  return count;
}

static int thiagofs_open(struct inode *inode, struct file *file)
{
  struct buffer_head * bh;
  struct thiagofs_inode * ti = NULL;
  struct file_contents *f;
  struct thiagofs_super_block *tsb;

  if (!(bh = sb_bread(inode->i_sb, inode->i_ino))) {
    printk("thiagofs: error: failed to read block %lu.\n", inode->i_ino);
    return -EINVAL;
  }
  
  f = thiagofs_find_file(inode);
  if (f == NULL) return -EIO;
  
  if (inode->i_ino == THIAGOFS_ROOT_INODE){   
    tsb = (struct thiagofs_super_block *) ((char *)bh->b_data);
    ti = &(tsb->root_inode);
  }
  else {
    ti = (struct thiagofs_inode *) ((char *)bh->b_data);
  }
  if(S_ISDIR(inode->i_mode)){
    struct thiagofs_directory_record *dr;
    dr = (struct thiagofs_directory_record *)(ti->data);
    for (; dr->dr_inode!=0; dr++)
      printk("open: dr->dr_inode=%d(%#x), dr->dr_ftype=%d, dr->dr_name=%s\n", dr->dr_inode, dr->dr_inode, dr->dr_ftype,dr->dr_name);
  }
  else {
    printk("open:file\n%s\n",ti->data);
  }
  if(ti && inode->i_size) 
    memcpy(f->conts, &(ti->data), inode->i_size);

  /*if (inode->i_private)
    file->private_data = ((struct thiagofs_inode *)inode->i_private)->data;
    */
  
  printk ("thiagofs: open!\n");
  return 0;
}

static int thiagofs_fsync(struct file * file, loff_t start, loff_t end, int datasync)
{
  return thiagofs_write_inode(file->f_inode, NULL);   
}

static int thiagofs_release(struct inode *inode, struct file *filp)
{
  thiagofs_sync_inode(inode);
  filp->private_data=NULL;
  return 0;
}

static const struct file_operations thiagofs_file_operations = {
  .read     = thiagofs_read,
  .write    = thiagofs_write,
  .open     = thiagofs_open,
  .fsync    = thiagofs_fsync,
  .release  = thiagofs_release,
  .readdir  = thiagofs_readdir,
};

