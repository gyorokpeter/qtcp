\l tcp.q

t:{.tcp.connect["alma";"localhost";9997]};
tf:{.tcp.connect["alma";"localhost";79]};

t2:{.tcp.send[alma;`byte$"\r\n"sv("GET /da.view HTTP/1.1";"Host: localhost";"Connection: close";"";"")]};

s:{.tcp.listen["server";9998]};
c:{.tcp.connect["client";"localhost";9998]};
su:{.udp.listen["server";9998]};
cu:{.udp.listen["server";9997]};
