/*
 * File-related system call implementations.
 */

#include <types.h>
#include <kern/errno.h>
#include <kern/fcntl.h>
#include <kern/limits.h>
#include <kern/seek.h>
#include <kern/stat.h>
#include <lib.h>
#include <uio.h>
#include <proc.h>
#include <current.h>
#include <synch.h>
#include <copyinout.h>
#include <vfs.h>
#include <vnode.h>
#include <openfile.h>
#include <filetable.h>
#include <syscall.h>

/*
 * open() - get the path with copyinstr, then use openfile_open and
 * filetable_place to do the real work.
 */
int
sys_open(const_userptr_t upath, int flags, mode_t mode, int *retval)
{
	const int allflags = O_ACCMODE | O_CREAT | O_EXCL | O_TRUNC | O_APPEND | O_NOCTTY;

	char *kpath;
	struct openfile *file;
	int result = 0;
	/*
	 * Your implementation of system call open starts here.
	 *
	 * Check the design document design/filesyscall.txt for the steps
	 */
	kpath = (char *) kmalloc(sizeof(char *)*PATH_MAX);
	if (kpath == NULL)
	{
		//kprintf("\nkpath error\n");
		*retval = -1;
		return ENOMEM;
	}
	//kprintf("\nkpath init\n");
	if (upath == NULL)
	{
		//kprintf("upath error\n");
		kfree(kpath);
		*retval = -1;
		return EFAULT;
	}
	if (flags > allflags)
	{
		//kprintf("flags error\n");
		kfree(kpath);
		*retval = -1;
		return EINVAL;
	}
	result = copyinstr(upath, kpath, PATH_MAX, NULL);
	//kprintf("copyinstr done.\n");
	if (result)
	{
		//kprintf("copy instr error\n");
		kfree(kpath);
		*retval = -1;
		return result;
	}

	result = openfile_open(kpath, flags, mode, &file);
	//kprintf("openfile done.\n");
	if (result)
	{
		//kprintf("openfile error\n");
		kfree(kpath);
		*retval = -1;
		return result;
	}
	result = filetable_place(curproc->p_filetable, file, retval);
	if (result)
	{
		//kprintf("filetable error\n");
		kfree(kpath);
		*retval = -1;
		return result;
	}
	//kprintf("All done here\n");
	kfree(kpath);
	//kprintf("\nsys_open\n");
	return 0;
	//(void) upath; // suppress compilation warning until code gets written
	//(void) flags; // suppress compilation warning until code gets written
	//(void) mode; // suppress compilation warning until code gets written
	//(void) retval; // suppress compilation warning until code gets written
	//(void) allflags; // suppress compilation warning until code gets written
	//(void) kpath; // suppress compilation warning until code gets written
	//(void) file; // suppress compilation warning until code gets written
}

/*
 * read() - read data from a file
 */
int
sys_read(int fd, userptr_t buf, size_t size, int *retval)
{
	int result = 0;
	struct openfile *file;
	struct iovec iv;
	struct uio uo;
	if (buf == NULL)
	{
		*retval = -1;
		return EFAULT;
	}
	result = filetable_get(curproc->p_filetable, fd, &file);
	if (result)
	{
		*retval = -1;
		return result;
	}
	
	//kprintf("filetable_get done\n");
	
	lock_acquire(file->of_offsetlock);
	//kprintf("lock acquired\n");

	if(file->of_accmode == O_WRONLY)
	{
		lock_release(file->of_offsetlock);
		*retval = -1;
		return EACCES;
	}
	
	uio_kinit(&iv, &uo, buf, size, file->of_offset, UIO_READ);
	//kprintf("uio initialized successfully!\n");
	
	result = VOP_READ(file->of_vnode, &uo);
	if (result)
	{
		lock_release(file->of_offsetlock);
		*retval = -1;
		return result;
	}
	
	file->of_offset = uo.uio_offset;
	lock_release(file->of_offsetlock);
	filetable_put(curproc->p_filetable, fd, file);
	*retval = 0;
	//kprintf("\nsys_read\n");
	return 0;
       /* 
        * Your implementation of system call read starts here.  
        *
        * Check the design document design/filesyscall.txt for the steps
        */
       /*(void) fd; // suppress compilation warning until code gets written
       (void) buf; // suppress compilation warning until code gets written
       (void) size; // suppress compilation warning until code gets written
       (void) retval; // suppress compilation warning until code gets written
	*/
}

/*
 * write() - write data to a file
*/ 
int sys_write(int fd, userptr_t buf, size_t size, int *retval)
{
	
	int result = 0;
	struct openfile *file;
	struct iovec iv;
	struct uio uo;
	//kprintf("in sys_write\n");
	if (buf == NULL)
	{
		*retval = -1;
		return EFAULT;
	}
	result = filetable_get(curproc->p_filetable, fd, &file);
	if (result)
	{
		*retval = -1;
		return result;
	}

	//kprintf("filetable_get done.\n");

	lock_acquire(file->of_offsetlock);
	//kprintf("lock acquired\n");
	
	if (file->of_accmode == O_RDONLY)
	{
		//lock_release(file->of_offsetlock);
		return EACCES;
	}

	uio_kinit(&iv, &uo, buf, size, file->of_offset, UIO_WRITE);
	/*(void) fd;
	(void) buf;
	(void) size;
	(void) retval;
	*/
	//kprintf("uio initialized\n");
	result = VOP_WRITE(file->of_vnode, &uo);
	if (result)	
	{
		//lock_release(file->of_offsetlock);
		return result;
	}
	//kprintf("VOP_WRITE successful\n");
	file->of_offset = uo.uio_offset;
	lock_release(file->of_offsetlock);
	filetable_put(curproc->p_filetable, fd, file);
	*retval = 0;
	//kprintf("\nsys_write\n");
	return 0;
}

/* close() - remove from the file table.
*/
int sys_close(int fd)
{
	int result = 0;
	struct openfile *file;
	//kprintf("in sys_close\n");
	if(!filetable_okfd(curproc->p_filetable, fd))
	{
		return EBADF;
	}
	//kprintf("fd number validated.\n");
	filetable_placeat(curproc->p_filetable, NULL, fd, &file);
	//kprintf("filetable_placeat replacing file table entry with NULL\n");
	if(curproc->p_filetable->ft_openfiles[fd-1] == NULL)
	{
		return EBADF;
	}
	if (file == NULL)
	{
		return ENOENT;
	}
	//kprintf("last file wasn't NULL\n");
	openfile_decref(file);
	//(void) fd;
	//kprintf("\nsys_close\n");
	return result;
}
/*
* meld () - combine the content of two files word by word into a new file
*/
