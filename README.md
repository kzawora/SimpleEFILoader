# Simple EFI Loader

Simple EFI Loader is a basic EFI application that tries to boot other .efi applications. 
It was done as a university project and therefore is _really_ basic and lacks many features. It is also the first EFI application I've ever done.
Tools used for development were 
- UDK2018 (stable release of edk2): https://github.com/tianocore/tianocore.github.io/wiki/UDK2018 
- VisualUefi: https://github.com/ionescu007/VisualUefi 

### What's inside
In this repo, you can find two versions of project, one for VisualUefi and other for UDK2018. They are similar, but VisualUefi is a bit outdated version, and the entire loader is made on top of simple sample Hello World project.
Both versions contain .efi binary and source code.
