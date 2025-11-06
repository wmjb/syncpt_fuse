# syncpt_fuse
bind /sys/devices/platform/tegra_grhost/syncpt/ to /sys/bus/nvhost/devices/host1x/syncpt using FUSE

build..

gcc -D_FILE_OFFSET_BITS=64 -Wall syncpt_fuse.c -o /usr/local/bin/syncpt_fuse -lfuse

sudo nano /etc/systemd/system/syncpt-setup.service
```
[Unit]
Description=Early syncpt FUSE setup
DefaultDependencies=no
Before=graphical.target
After=local-fs.target

[Service]
Type=oneshot
ExecStart=/bin/bash -c '\
  mkdir -p /tmp/fake_sysfs/bus/nvhost/devices/host1x && \
  sudo -u ubuntu /usr/local/bin/syncpt_fuse -o allow_other,nonempty /tmp/fake_sysfs/bus/nvhost/devices/host1x & \
  sleep 1 && \
  while [ ! -d /tmp/fake_sysfs/bus/nvhost/devices/host1x/syncpt ]; do sleep 0.5; done && \
  mount --bind /tmp/fake_sysfs/bus/nvhost/devices /sys/bus/nvhost/devices && \
  mount --rbind /tmp/fake_sysfs/bus/nvhost/devices/host1x /sys/bus/nvhost/devices/host1x'
RemainAfterExit=yes

[Install]
WantedBy=multi-user.target
```

sudo systemctl daemon-reload

sudo systemctl enable syncpt-setup.service

sudo nano /etc/systemd/system/syncpt-fuse.service
```
[Unit]
Description=Syncpt FUSE Emulation
After=local-fs.target
Requires=local-fs.target

[Service]
ExecStart=/usr/local/bin/run_syncpt_fuse.sh
Restart=always
User=root

[Install]
WantedBy=multi-user.target
```

sudo nano /usr/local/bin/run_syncpt_fuse.sh
```
#!/bin/bash
mkdir -p /tmp/fake_sysfs/bus/nvhost/devices/host1x
exec /usr/local/bin/syncpt_fuse -f -o allow_other,nonempty /tmp/fake_sysfs/bus/nvhost/devices/host1x
```
sudo chmod +x /usr/local/bin/run_syncpt_fuse.sh

sudo systemctl daemon-reload

sudo systemctl enable syncpt-fuse.service
