# os-team20

## 자주 쓰는 커맨드

### SDB 사용하는 법
먼저 정상적으로 artic 부팅하고 root로 로그인, 다음 커맨드를 입력.
```sh
direct_set_debug.sh --sdb-set
```

우분투 환경에서 sdb root로 전환 후 push
```sh
sdb root on
push [원본파일] [destination]
```

### printk 출력 레벨 변경
```sh
echo 8 > /proc/sys/kernel/printk
```
이렇게 하면 모든 메시지가 콘솔에 출력됨
