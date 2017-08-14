
/*
 * Copyright (C) Igor Sysoev
 * Copyright (C) Nginx, Inc.
 */


#ifndef _NGX_CYCLE_H_INCLUDED_
#define _NGX_CYCLE_H_INCLUDED_


#include <ngx_config.h>
#include <ngx_core.h>


#ifndef NGX_CYCLE_POOL_SIZE
#define NGX_CYCLE_POOL_SIZE     NGX_DEFAULT_POOL_SIZE
#endif


#define NGX_DEBUG_POINTS_STOP   1
#define NGX_DEBUG_POINTS_ABORT  2


typedef struct ngx_shm_zone_s  ngx_shm_zone_t;

typedef ngx_int_t (*ngx_shm_zone_init_pt) (ngx_shm_zone_t *zone, void *data);

struct ngx_shm_zone_s {
    void                     *data;
    ngx_shm_t                 shm;
    ngx_shm_zone_init_pt      init;
    void                     *tag;
    ngx_uint_t                noreuse;  /* unsigned  noreuse:1; */
};


//初始化参考ngx_init_cycle，最终有一个全局类型的ngx_cycle_s，即ngx_cycle,  ngx_conf_s中包含该类型成员cycle
struct ngx_cycle_s {
	/*保存着所有模块存储配置项的结构体指针，     它首先是一个数组，数组大小为ngx_max_module，正好与Nginx的module个数一样；     
	每个数组成员又是一个指针，指向另一个存储着指针的数组，因此会看到void ****    请见陶辉所著《深入理解Nginx-模块开发与架构解析》
	 一书302页插图。    另外，这个图也不错：http://img.my.csdn.net/uploads/201202/9/0_1328799724GTUk.gif 该数组的成员数为ngx_max_module   
	*/ 
					        
	 /* 例如http核心模块的conf_ctx[ngx_http_module->index]=ngx_http_conf_ctx_t,见ngx_conf_handler,ngx_http_block 见ngx_init_cycle  conf.ctx = cycle->conf_ctx; //这样下面的ngx_conf_param解析配置的时候，里面对conf.ctx赋值操作，实际上就是对cycle->conf_ctx[i]可如何由ngx_cycle_t核心结构体中找到main级别的配置结构体呢？Nginx提供的ngx_http_cycle_get_module_main_conf宏可以实现这个功能
	*/ 
	
	/*
	图形化参考:深入理解NGINX中的图9-2(P302)  图10-1(P353) 图10-1(P356) 图10-1(P359) 图4-2(P145)
	ngx_http_conf_ctx_t、ngx_http_core_main_conf_t、ngx_http_core_srv_conf_t、ngx_http_core_loc_conf_s和ngx_cycle_s->conf_ctx的关系见:
	Nginx的http配置结构体的组织结构:http://tech.uc.cn/?p=300
	*/ 
    void                  ****conf_ctx;
    ngx_pool_t               *pool; // 内存池

	/*  日志模块中提供了生成基本ngx_log_t日志对象的功能，这里的log实际上是在还没有执行ngx_init_cycle方法前，    
    也就是还没有解析配置前，如果有信息需要输出到日志，就会暂时使用log对象，它会输出到屏幕。    
	在ngx_init_cycle方法执行后，将会根据nginx.conf配置文件中的配置项，构造出正确的日志文件，此时会对log重新赋值。   
	*/
	//ngx_init_cycle中赋值cycle->log = &cycle->new_log;
	//指向ngx_log_init中的ngx_log，如果配置error_log，指向这个配置后面的文件参数，见ngx_error_log。否则在ngx_log_open_default中设置
    ngx_log_t                *log;

	/* 由nginx.conf配置文件读取到日志文件路径后，将开始初始化error_log日志文件，由于log对象还在用于输出日志到屏幕,
	这时会用new_log对象暂时性地替代log日志，待初始化成功后，会用new_log的地址覆盖上面的log指针
	*/
	// 如果没有配置error_log则在ngx_log_open_default设置为NGX_ERROR_LOG_PATH，如果通过error_log有配置过则通过ngx_log_set_log添加到该new_log->next链表连接起来
	/* 全局中配置的error_log xxx存储在ngx_cycle_s->new_log，http{}、server{}、local{}配置的error_log保存在ngx_http_core_loc_conf_t->error_log,
    见ngx_log_set_log,如果只配置全局error_log，不配置http{}、server{}、local{}则在ngx_http_core_merge_loc_conf conf->error_log = &cf->cycle->new_log;  */
 	//ngx_log_insert插入，在ngx_log_error_core找到对应级别的日志配置进行输出，因为可以配置error_log不同级别的日志存储在不同的日志文件中
 	//如果配置error_log，指向这个配置后面的文件参数，见ngx_error_log。否则在ngx_log_open_default中设置
    ngx_log_t                 new_log;

    ngx_uint_t                log_use_stderr;  /* unsigned  log_use_stderr:1; */

	/*  对于poll，rtsig这样的事件模块，会以有效文件句柄数来预先建立这些ngx_connection t结构
	体，以加速事件的收集、分发。这时files就会保存所有ngx_connection_t的指针组成的数组，files_n就是指
	针的总数，而文件句柄的值用来访问files数组成员 */
    ngx_connection_t        **files; //sizeof(ngx_connection_t *) * cycle->files_n  见ngx_event_process_init  ngx_get_connection
    /*
    从图9-1中可以看出，在ngx_cycle_t中的connections和free_connections达两个成员构成了一个连接池，其中connections指向整个连
    接池数组的首部，而free_connections则指向第一个ngx_connection_t空闲连接。所有的空闲连接ngx_connection_t都以data成员（见9.3.1节）作
    为next指针串联成一个单链表，如此，一旦有用户发起连接时就从free_connections指向的链表头获取一个空闲的连接，同时free_connections再指
    向下一个空闲连接。而归还连接时只需把该连接插入到free_connections链表表头即可。
     */
    //见ngx_event_process_init, ngx_connection_t空间和它当中的读写ngx_event_t存储空间都在该函数一次性分配好
    ngx_connection_t         *free_connections; //可用连接池，与free_connection_n配合使用
    ngx_uint_t                free_connection_n; //可用连接池中连接的总数

	//ngx_connection_s中的queue添加到该链表上
	/* 双向链表容器，元素类型是ngx_connection_t结构体，表示可重复使用连接队列 表示可以重用的连接 */
    ngx_queue_t               reusable_connections_queue;

	//通过"listen"配置创建ngx_listening_t加入到该数组中
    ngx_array_t               listening; // 动态数组，每个数组元素储存着ngx_listening_t成员，表示监听端口及相关的参数
    ngx_array_t               paths;
    ngx_array_t               config_dump;
    ngx_list_t                open_files;
    ngx_list_t                shared_memory;

    ngx_uint_t                connection_n;
    ngx_uint_t                files_n;

    ngx_connection_t         *connections;
    ngx_event_t              *read_events;
    ngx_event_t              *write_events;

    ngx_cycle_t              *old_cycle;

    ngx_str_t                 conf_file; //配置文件相对于安装目录的路径名称 默认为安装路径下的NGX_CONF_PATH,见ngx_process_options 
    ngx_str_t                 conf_param;
    ngx_str_t                 conf_prefix; // nginx配置文件所在目录的路径  ngx_prefix 见ngx_process_options
    ngx_str_t                 prefix; //nginx安装目录的路径 ngx_prefix 见ngx_process_options
    ngx_str_t                 lock_file;
    ngx_str_t                 hostname;
};


typedef struct {
     ngx_flag_t               daemon;
     ngx_flag_t               master;

     ngx_msec_t               timer_resolution;

     ngx_int_t                worker_processes;
     ngx_int_t                debug_points;

     ngx_int_t                rlimit_nofile;
     off_t                    rlimit_core;

     int                      priority;

     ngx_uint_t               cpu_affinity_n;
     uint64_t                *cpu_affinity;

     char                    *username;
     ngx_uid_t                user;
     ngx_gid_t                group;

     ngx_str_t                working_directory;
     ngx_str_t                lock_file;

     ngx_str_t                pid;
     ngx_str_t                oldpid;

     ngx_array_t              env;
     char                   **environment;
} ngx_core_conf_t;


#define ngx_is_init_cycle(cycle)  (cycle->conf_ctx == NULL)


ngx_cycle_t *ngx_init_cycle(ngx_cycle_t *old_cycle);
ngx_int_t ngx_create_pidfile(ngx_str_t *name, ngx_log_t *log);
void ngx_delete_pidfile(ngx_cycle_t *cycle);
ngx_int_t ngx_signal_process(ngx_cycle_t *cycle, char *sig);
void ngx_reopen_files(ngx_cycle_t *cycle, ngx_uid_t user);
char **ngx_set_environment(ngx_cycle_t *cycle, ngx_uint_t *last);
ngx_pid_t ngx_exec_new_binary(ngx_cycle_t *cycle, char *const *argv);
uint64_t ngx_get_cpu_affinity(ngx_uint_t n);
ngx_shm_zone_t *ngx_shared_memory_add(ngx_conf_t *cf, ngx_str_t *name,
    size_t size, void *tag);


extern volatile ngx_cycle_t  *ngx_cycle;
extern ngx_array_t            ngx_old_cycles;
extern ngx_module_t           ngx_core_module;
extern ngx_uint_t             ngx_test_config;
extern ngx_uint_t             ngx_dump_config;
extern ngx_uint_t             ngx_quiet_mode;


#endif /* _NGX_CYCLE_H_INCLUDED_ */
