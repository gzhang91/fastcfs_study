### 1. libfastcommon

```
git clone https://github.com/happyfish100/libfastcommon.git; cd libfastcommon/
git checkout master
./make.sh clean && ./make.sh && ./make.sh install
```

default install directories:
```
/usr/lib64
/usr/lib
/usr/include/fastcommon
```

### 2. libserverframe

```
git clone https://github.com/happyfish100/libserverframe.git; cd libserverframe/
./make.sh clean && ./make.sh && ./make.sh install
```

### 3. fastDIR

```
git clone https://github.com/happyfish100/fastDIR.git; cd fastDIR/
./make.sh clean && ./make.sh && ./make.sh install
```

### 4. faststore

```
git clone https://github.com/happyfish100/faststore.git; cd faststore/
./make.sh clean && ./make.sh && ./make.sh install
mkdir /etc/fstore/
cp conf/server.conf conf/client.conf conf/servers.conf conf/cluster.conf conf/storage.conf /etc/fstore/
```


### 5. libfuse

libfuse requires meson and ninja which require python3.5 or higher version

##### python

packages: python3  python3-pip

Ubuntu:
```
apt install python3 python3-pip -y
```

CentOS:
```
yum install python3 python3-pip -y
```

##### meson and ninja

```
pip3 install meson
pip3 install ninja
```

##### gcc

Ubuntu:
```
apt install gcc g++ -y
```

CentOS:
```
yum install gcc gcc-c++ -y
```

##### libfuse

```
git clone https://github.com/libfuse/libfuse.git
cd libfuse/
git checkout fuse-3.10.1
mkdir build/; cd build/
meson ..
meson configure -D prefix=/usr
meson configure -D examples=false
ninja && ninja install
sed -i 's/#user_allow_other/user_allow_other/g' /etc/fuse.conf
```

### 6. FastCFS

```
git clone https://github.com/happyfish100/FastCFS.git; cd FastCFS/
./make.sh clean && ./make.sh && ./make.sh install
mkdir /etc/fcfs/
cp conf/fuse.conf /etc/fcfs/
```
