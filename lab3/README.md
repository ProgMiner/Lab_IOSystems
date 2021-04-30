# Лабораторная работа №3
**Название** "Разработка сетевого драйвера" 
### Цель работы
получить знания и навыки разработки драйверов сетевых интерфейсов для операционной системы Linux.

### Описание функциональности драйвера
Перехватываемый трафик: Пакеты протокола ICMP (Internet Control Message Protocol) – только тип 8. Вывести данные. 

Состояние разбора пакетов необходимо выводить в файл в директории `/proc`


## Инструкция по сборке
**требуемая версия ядра:** 4.15

Выполнить
```bash
sudo apt install build-essential linux-headers-`uname -r`
make
```

## Инструкция пользователя

для загрузки модуля выпонлить 
```bash
sudo make load
```

Проверить, всё ли нормально загрузилось 
```bash
make watch
```
Должно быть что-то вроде этого:
```bash
[ 1327.955154] Module main loaded
[ 1327.955155] main: create link vni0
[ 1327.955156] main: registered rx handler for enp0s3
[ 1327.983799] IPv6: ADDRCONF(NETDEV_UP): vni0: link is not ready
[ 1327.983835] main: device opened: name=vni0
```

Проверяем созданный сетевой интерфейс 
```bash
ip a
```
```bash
1: lo: <LOOPBACK,UP,LOWER_UP> mtu 65536 qdisc noqueue state UNKNOWN group default qlen 1000
    link/loopback 00:00:00:00:00:00 brd 00:00:00:00:00:00
    inet 127.0.0.1/8 scope host lo
       valid_lft forever preferred_lft forever
    inet6 ::1/128 scope host 
       valid_lft forever preferred_lft forever
2: enp0s3: <BROADCAST,MULTICAST,UP,LOWER_UP> mtu 1500 qdisc fq_codel state UP group default qlen 1000
    link/ether 08:00:27:58:ee:60 brd ff:ff:ff:ff:ff:ff
    inet 10.0.2.15/24 brd 10.0.2.255 scope global dynamic noprefixroute enp0s3
       valid_lft 84867sec preferred_lft 84867sec
    inet6 fe80::c166:6131:9254:4736/64 scope link noprefixroute 
       valid_lft forever preferred_lft forever
3: lab3: <BROADCAST,MULTICAST,UP,LOWER_UP> mtu 1500 qdisc fq_codel state UNKNOWN group default qlen 1000
    link/ether 08:00:27:58:ee:60 brd ff:ff:ff:ff:ff:ff
```


Смотрим есть ли в `/proc`

```bash
ls /proc | grep lab3
```
если есть, значит все создалось правильно

для выгрузки модуля 
```bash
sudo make unload
```
## Примеры использования

```shell
ping -c 2 -p 486f636875 -s 7 10.0.2.15
ping -c 2 -p 417262757a -s 6 10.0.2.15
```
#### Проверяем содержимое `/proc/lab3`
```shell
cat /proc/lab3
```

```shell
1. 5 bytes:
Bytes: "hochu"
Hex: 68 6F 63 68 75
2. 5 bytes:
Bytes: "hochu"
Hex: 68 6F 63 68 75
3. 5 bytes:
Bytes: "Arbuz"
Hex: 41 72 62 75 7A
4. 5 bytes:
Bytes: "Arbuz"
Hex: 41 72 62 75 7A
Summary: 20 bytes.
```
#### Смотрим статистику принятых пакетов
```shell
ip -s link show lab3
```
```shell
 mode DEFAULT group default qlen 1000
    link/ether 08:00:27:58:ee:60 brd ff:ff:ff:ff:ff:ff
    RX: bytes  packets  errors  dropped overrun mcast   
    184        4        0       0       0       0       
    TX: bytes  packets  errors  dropped carrier collsns 
    0          0        0       0       0       0     
```


