# ops-highload-systems-lab3

## Admin task

На двух ВМ cоздать виртуальное устройство dummy0 и настроить на нём /32 адрес из приватного
диапазона
1. Статически (файлом конфигурации в systemd-networkd)
   * [dummy0.network](network/dummy0.network)
   * [dummy0.netdev](network/dummy0.netdev)
2. Настроить маршрутизацию между /32
   * [bird + BGP](bird/bird.conf)
3. Обменяться данными между /32 адресами этих ВМ  
  `ip a sh dev dummy0`  
   ```  
   3: dummy0: <BROADCAST,NOARP,UP,LOWER_UP> mtu 1500 qdisc noqueue state UNKNOWN group default qlen 1000
     link/ether be:1b:6a:61:46:f4 brd ff:ff:ff:ff:ff:ff
     inet 192.168.1.1/32 scope global dummy0
        valid_lft forever preferred_lft forever
     inet6 fe80::bc1b:6aff:fe61:46f4/64 scope link
        valid_lft forever preferred_lft forever
   ```  
   `birdc show route table master4`
   ```
     BIRD 2.15.1 ready.
     Table master4:
     192.168.1.2/32       unicast [uplink1 00:59:02.475] * (100) [AS65000i]
             via 192.168.0.105 on enp0s3
     192.168.1.1/32       unicast [direct1 00:48:48.428] * (240)
             dev dummy0
   ```
   `birdc show proto`
   ```
   BIRD 2.15.1 ready.
   Name       Proto      Table      State  Since         Info
   device1    Device     ---        up     00:48:48.427
   direct1    Direct     ---        up     00:48:48.427
   kernel1    Kernel     master4    up     00:48:48.427
   kernel2    Kernel     master6    up     00:48:48.427
   static1    Static     master4    up     00:48:48.427
   uplink1    BGP        ---        up     00:59:02.428  Established
   ```
   `ping 192.168.1.2 -c 4`
   ```
   PING 192.168.1.2 (192.168.1.2) 56(84) bytes of data.
   64 bytes from 192.168.1.2: icmp_seq=1 ttl=65 time=0.274 ms
   64 bytes from 192.168.1.2: icmp_seq=2 ttl=65 time=0.255 ms
   64 bytes from 192.168.1.2: icmp_seq=3 ttl=65 time=0.543 ms
   64 bytes from 192.168.1.2: icmp_seq=4 ttl=65 time=0.684 ms

   --- 192.168.1.2 ping statistics ---
   4 packets transmitted, 4 received, 0% packet loss, time 3080ms
   rtt min/avg/max/mdev = 0.255/0.439/0.684/0.181 ms
   ```
   `ip r`
   ```
   default via 192.168.0.1 dev enp0s3 proto dhcp src 192.168.0.157 metric 100
   192.168.0.0/24 dev enp0s3 proto kernel scope link src 192.168.0.157 metric 100
   192.168.1.1 dev dummy0 proto bird scope link metric 32
   192.168.1.2 via 192.168.0.105 dev enp0s3 proto bird metric 32
   ```

   ```
   systemctl stop bird
   ip r
   ```

   ```
   default via 192.168.0.1 dev enp0s3 proto dhcp src 192.168.0.157 metric 100
   192.168.0.0/24 dev enp0s3 proto kernel scope link src 192.168.0.157 metric 100
   ```

   `ping 192.168.1.2 -c 4`
   ```
   PING 192.168.1.2 (192.168.1.2) 56(84) bytes of data.
   ^C
   --- 192.168.1.2 ping statistics ---
   4 packets transmitted, 0 received, 100% packet loss, time 3072ms
   ```
