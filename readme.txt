Working of yashd daemon and yash client 

1. Compile, run and clean using Makefile-

    a. To compile yash and yashd, type $make on terminal.It will build both the programms.
        command : $make
    b. To clean the output files of yash client and yashd daemon run command $make clean. 
        command : $make clean   
    c. To kill yashd daemon run the command: $ kill -9 $(cat /tmp/yashd.pid)

2. Yashd daemon- 
    
    a. yashd daemon can be run with the command : $./yashd
    b. Port is set to 3826 in the client and server. (given in project document)
    c. yashd directory has been set to current working directory.
    d. Buffer size is 1024.
    e. Log file is placed under /tmp directory and can be accessed using command : $cat /tmp/yashd.log
    f. All the commands(CMD) and control(CTL siganls) issued by the client are logged in the log file.
    g. Any errors occured during the execution of the commands are redirected to client terminal.
    h. PID file is placed under /tmp directory and can be accessed using command : $ cat /tmp/yashd.pid

3. Yash client-

    a. yash client can be run with the command : $./yash localhost
    b. yash client expects the commands same as a normal terminal and internally it appends CMD(for the 
        commands like ls, echo) and CTL(for signals ctl+z, ctl+c). You do NOT need to type CMD and CTL 
        along with commands, it is done internally.
    c. Buffer size is 1024.    
    d. When client is connected to the server it will get "\n#\n" from the server instead of "\n#". That 
        means you will get an extra newline from the server.
