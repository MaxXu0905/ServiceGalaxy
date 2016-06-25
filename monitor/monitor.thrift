namespace cpp ai.sg
namespace java com.ailk.monitor

struct ClusterInfo {
	1:i32 ipckey,
	2:i32 uid,
	3:i32 gid,
	4:i32 perm,
	5:i32 options,
	6:string master,
	7:i32 current,
	8:i32 maxpes,
	9:i32 maxnodes,
	10:i32 maxaccsrs,
	11:i32 maxques,
	12:i32 maxsgt,
	13:i32 maxsvrs,
	14:i32 maxsvcs,
	15:i32 quebkts,
	16:i32 sgtbkts,
	17:i32 svrbkts,
	18:i32 svcbkts,
	19:i32 scan_unit,
	20:i32 sanity_scan,
	21:i32 stack_size,
	22:i64 max_num_msg,
	23:i64 max_msg_size,
	24:i64 block_time,
	25:i64 dbbm_wait,
	26:i64 bbm_query
}
struct MachineNet{
	1:string lmid,
	2:string naddr,
	3:string nlsaddr,
	4:string netgroup,
	5:string netid,
	6:bool tcpkeepalive
}
struct MachineInfo{
	1:string pmid,
	2:string lmid,
	3:string sgdir,
	4:string appdir,
	5:string sgconfig,
	6:string envfile,
	7:string ulogpfx,
	8:i32 mid,
	9:i32 flags,
	10:i32 maxaccsrs,
	11:i32 perm,
	12:i32 uid,
	13:i32 gid,
	14:i64 net_load,
	15:list<MachineNet> nets
}

typedef i32 BBVERSION

struct QueueInfo{
	1:i64 sgid,
	2:i32 options,
	3:i32 grace,
	4:i32 maxgen,
	5:i32 paddr,
	6:string saddr,
	7:string rcmd,
	8:string filename
}
struct ServerGroup{
	1:i64 sgid,
	2:i32 grpid,
	3:string sgname,
	4:i32 curmid,
	5:string lmid
}

struct ServerInfo{
	1:i64 sgid,
	2:i64 queuesgid,
	3:i32 grpid,
	4:i32 svrid,
	5:string servname
}
struct ServiceInfo{
	1:i64 sgid,
	2:i32 grpid,
	4:i32 serverid,
	5:i32 priority,
	6:string svc_name,
	7:string svc_cname,
	8:i32 svc_index,
	9:i32 svc_load,
	10:i32 load_limit,
	11:i64 svctimeout,
	12:i64 svcblktime
}

struct ClusterState{
	1:i64 currload,
	2:i32 cmachines,
	3:i32 cques,
	4:i32 csvrs,
	5:i32 csvcs,
	6:i32 csgt,
	7:i32 caccsrs,
	8:i32 maxmachines,
	9:i32 maxques,
	10:i32 maxsvrs,
	11:i32 maxsvcs,
	12:i32 maxsgt,
	13:i32 maxaccsrs,
	14:i32 wkinitiated,
	15:i32 wkcompleted,
	16:i32 rreqmade
}

struct QueueState{
	1:i64 sgid,
	2:string saddr,
	3:i32 totnqueued,
	4:i32 nqueued,
	5:i64 totwkqueued,
	6:i64 wkqueued,
	7:i32 status,
	8:i32 svrcnt,
	9:i32 spawn_state
}
struct ServerState{
	1:i64 sgid,
	2:i32 serverid,
	3:i32 total_idle,
	4:i32 total_busy,
	5:i64 ncompleted,
	6:i64 wkcompleted,
	7:i64 ctime,
	8:i32 status,
	9:i64 mtime,
	10:i64 svridmax
}
struct ServiceState{
	1:i64 sgid,
	2:i32 ncompleted,
	3:i32 nqueued,
	4:i32 status
}
struct AllInfo{
	1:ClusterInfo clusterInfo,
	2:ClusterState clusterState,
	3:list<MachineInfo> machineInfos,
	4:BBVERSION bbversion,
	5:list<ServerGroup> serverGroups,
	6:list<ServerInfo> serverInfos,
	7:list<ServerState> serverStates,
	8:list<ServiceInfo> serviceInfos,
	9:list<ServiceState> serviceStates,
	10:list<QueueInfo> queueInfos,
	11:list<QueueState> queueStates
	
}
enum Operate
{
  Start=1,
  Stop=2,
  Suspend=3,
  Resume=4,
}
enum OperateType
{
  Server = 1,
  ServerGroup=2,
  Machine=3,
  All = 4
}
struct ServerOperate{
	1:OperateType type,
	2:Operate operate,
	3:optional i32 groupId,
	4:optional i32 serverId,
	5:optional string machineId
}
service Monitor{
	ClusterInfo GetClusterInfo(),
	ClusterState GetClusterState(),
	
	list<MachineInfo> GetMachineInfoList(),
	
	BBVERSION getBBVersion(),
	
	list<ServerGroup> GetServerGroupList(),
	list<ServerInfo> GetServerInfoList(),
	
	list<ServerState> GetServerStateList(),
	
	list<ServiceInfo> GetServiceInfoList(),
	list<ServiceState> GetServiceStateList(),
	
	list<QueueInfo> GetQueueInfoList(),
	list<QueueState> GetQueueStateList(),
	
	AllInfo getAllInfo(1:BBVERSION bbversion),
	
	i32 startOrStopServer(1:list<ServerOperate> operates),
	i32 updateServer(1:Operate operate,2:list<i64> sgids),
	i32 updateQueue(1:Operate operate,2:list<i64> sgids),
	i32 updateService(1:Operate operate,2:list<i64> sgids)
}