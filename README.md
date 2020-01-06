Steps:
```
sudo su
make  # look into Makefile for no surprises
cat ./data.xml | xclip -selection c
sysrepocfg -E -d running -m model
*Paste and write*
```
