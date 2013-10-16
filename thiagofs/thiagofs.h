/*
 * thiagofs: minimalist (possibly non-functional) persistent filesystem.
 * Made for educational purposes.
 * Built upon thiagofsv2 [https://github.com/thiagodecasantiago/mo806],
 * which was built upon thiagofs [http://www.ic.unicamp.br/~islene/2s2013-mo806/vfs/vfs.html].
 * Influenced by gogthiagofs [shttp://www.ic.unicamp.br/~islene/2s2013-mo806/vfs/gogthiagofs/],
 * STAMFS [http://haifux.org/lectures/120/] (heavily),
 * and ext2 [https://www.kernel.org/].
 * Works with kernel version 3.10.x. Mountable using a file as a disk.
 */

#ifndef _THIAGOFS_H_
#define _THIAGOFS_H_

#define THIAGOFS_SUPER_MAGIC    0x072488

#define THIAGOFS_MAX_FILESIZE   3072 // (3 kB)
#define THIAGOFS_MAX_NAME_SIZE  24

#define THIAGOFS_ROOT_INODE     1
#define THIAGOFS_JOURNAL_INODE  2

struct thiagofs_inode {
  __u32 i_ino;
  __u32 i_blocks;
  __u16 i_uid;
  __u16 i_gid;
  __u16 i_mode;
  __u32 i_mtime;
  __u32 i_atime;
  __u32 i_ctime;
  __u16 i_size;
  __u8  data[THIAGOFS_MAX_FILESIZE];
};

struct thiagofs_directory_record {
  __u32 dr_inode;
  __u8  dr_ftype;
  char  dr_name[THIAGOFS_MAX_NAME_SIZE];
};

struct thiagofs_super_block {
  __u32 s_magic;
  __u32 s_max_inode_number_in_use;
  struct thiagofs_inode root_inode;
};

/* Lembre-se que nao temos um disco! (Isso so complicaria as coisas, pois teriamos
 * que lidar com o sub-sistema de I/O. Entao teremos uma representacao bastante
 * simples da estrutura de arquivos: Uma lista duplamente ligada circular (para
 * aplicacoes reais, um hash seria muito mais adequado) contem em cada elemento
 * um indice (inode) e uma pagina para conteudo (ou seja: o tamanho maximo de um
 * arquivo nessa versao do islene fs eh de 4Kb. Nao ha subdiretorios */
struct file_contents {
  struct list_head list;
  struct inode *inode;
  void *conts;
};

/* lista duplamente ligada circular, contendo todos os arquivos do fs */
static LIST_HEAD(contents_list);



/* file.c */
static const struct file_operations thiagofs_file_operations;

/* inode.c */
static const struct inode_operations thiagofs_dir_inode_operations;
static const struct inode_operations thiagofs_file_inode_operations;

struct file_contents *thiagofs_find_file(struct inode *inode);
struct inode *thiagofs_find_inode_by_ino(unsigned long ino);
int thiagofs_sync_inode(struct inode * inode);
int thiagofs_dir_add_link(struct inode *parent_dir, struct inode *child, const char *name, int namelen);
int thiagofs_readdir(struct file *filp, void *dirent, filldir_t filldir);


/* journal.c */
int thiagofs_write_journal(char * buf);
int thiagofs_create_journal(struct super_block *sb, struct dentry *root);

/* super.c */

int thiagofs_write_inode(struct inode * inode, struct writeback_control *wbc);

#endif /* _THIAGOFS_H_  */
