> void SysHalt(struct intr_frame* )

函数调用了`void shutdown_power_off (void)` ，只要我们用的是Bochs或QEMU，就关闭正在运行的机器。

> void SysCreate(struct intr_frame *f)

函数调用流程图如下：

![未命名文件(2)](C:\Users\24951\Desktop\未命名文件(2).png)

> void SysOpen(struct intr_frame *f)

函数调用流程图如下：

![未命名文件(3)](C:\Users\24951\Desktop\未命名文件(3).png)

> void SysClose(struct intr_frame *f)

函数调用流程图如下：

![未命名文件(4)](C:\Users\24951\Desktop\未命名文件(4).png)

> void SysFilesize(struct intr_frame *f)

函数调用流程图如下：

![未命名文件(5)](C:\Users\24951\Desktop\未命名文件(5).png)

> void SysSeek(struct intr_frame *f)

函数调用流程图如下：

![未命名文件(6)](C:\Users\24951\Desktop\未命名文件(6).png)

> void SysRemove(struct intr_frame *f)

函数调用流程图如下：



![未命名文件(7)](C:\Users\24951\Desktop\未命名文件(7).png)

测试样例的分析：

`tests/userprog/halt`样例测试的是系统中关于终端的系统调用。测试函数中仅调用了`void halt (void) NO_RETURN`函数，如果函数调用没有被中断，则接下来会输出`should have halted`的报错。

`tests/userprog/create-bad-ptr`样例中，测试函数会传递给一个坏指针给创建文件的系统调用函数，并且应当造成进程以退出码-1终止退出。

`tests/userprog/create-bound`样例中，测试函数会打开一个名称跨越两个页面之间的边界的文件。这应该是是有效的，所以它必须成功。

`tests/userprog/create-empty`样例中，测试函数尝试创建一个以空字符串作为文件名的文件。

`tests/userprog/create-exists`样例中，测试函数会尝试创建一个文件名已经存在的文件，并且证实这次创建是无效的。

`tests/userprog/create-long`样例中，测试函数尝试创建一个文件名过长的文件，这个尝试应当是失败的。

`tests/userprog/create-normal`样例中，测试函数仅仅是尝试创建一个普通的文件，测试正常情况下的文件创建功能。

`tests/userprog/create-null`样例中，测试函数尝试创建一个文件以空指针作为其文件名的文件，并且此时进程应该以退出码-1推出终止。