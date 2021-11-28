{
    path:"/"sv -1_"/"vs ssr[;"\\";"/"]first -3#value .z.s;
    .tcp.priv.lib:`$":",path,"/qtcp";
    .tcp.connect:.tcp.priv.lib 2:(`qtcp_connect;3);
    .tcp.listen:.tcp.priv.lib 2:(`qtcp_listen;2);
    .tcp.close:.tcp.priv.lib 2:(`qtcp_close;1);
    .tcp.send:.tcp.priv.lib 2:(`qtcp_send;2);
    .udp.listen:.tcp.priv.lib 2:(`qudp_listen;2);
    .udp.send:.tcp.priv.lib 2:(`qudp_send;4);
    }[]

//CALLBACKS - to be overwritten by user

.tcp.connFailed:{[alias;msg]
    -1".tcp.connFailed: ",alias," - ",msg;
    };

.tcp.connSuccess:{[alias;handle]
    -1".tcp.connSuccess: ",alias," - ",string handle;
    set[`$alias;handle];
    };

.tcp.listenSuccess:{[alias;protocol;handle]
    -1".tcp.listenSuccess: ",alias," v",string[protocol]," - ",string handle;
    set[`$alias,"_v",string[protocol];handle];
    };

.tcp.clientConnect:{[listenHandle;handle;address]
    -1".tcp.clientConnect: ph ",string[listenHandle]," h ",string[handle]," host ",address;
    };

.tcp.disconnect:{[handle]
    -1".tcp.disconnect: ",string handle;
    };

.tcp.receive:{[handle;msg]
    cmsg:`char$msg;
    -1".tcp.receive: ",string[handle]," - ",cmsg;
    if[cmsg like "GET / HTTP/1.1*";
        .tcp.send[handle;`byte$.h.hy[`txt]"nothing to see here"];
    ];
    if[cmsg like "GET /favicon.ico HTTP/1.1*";
        .tcp.send[handle;`byte$.h.hn["404 Not Found";`txt;"The requested object was not found on this server."]];
    ];
    };

.udp.listenSuccess:{[alias;handle]
    -1".udp.listenSuccess: ",alias," - ",string handle;
    set[`$alias;handle];
    };

.udp.receive:{[handle;host;port;msg]
    -1".udp.receive: h ",string[handle]," host ",("."sv string`int$0x00 vs host)," port ",string[port]," msg=",`char$msg;
    };
