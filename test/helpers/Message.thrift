namespace cpp ai.test

service Message{
	i32 send(1:string key,2:string msg),
	i32 sendadd(1:string key,2:string msg)
}