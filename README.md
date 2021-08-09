# space_invaders

A Space Invaders arcade emulator written in C++. Still a WIP!

<img src="/preview.png" width="752" height="646">

# Build from source

First ensure that the space invaders rom files are in the data directory. The following files are required:
* invaders.e
* invaders.f
* invaders.g
* invaders.h

Clone the repository:

```shell
git clone https://github.com/adelciotto/space_invaders
cd space_invaders
```

## OSX

Compile the code:

```shell
make -j4
```

## Windows

Ensure you have the latest Visual Studio installed and that you have run vcvars64.bat in your current command line session. The scripts/shell.bat script will attempt to run this for you assuming you have Visual Studio 2019 Community installed. If you have another version installed then just modify the script to point to the right location.

```batch
scripts\shell.bat
```

Compile the code:

```batch
scripts\windows_build.bat
```

Run the executable:

```batch
release\space_invaders.exe
```

## Linux

TODO!

## Controls

### Space Invaders 

#### Keyboard

| Action         | Key         |
|----------------|-------------|
| Move Left      | Left Arrow  |
| Move Right     | Right Arrow |
| Shoot          | Z           |
| Insert Coin    | I           |
| Start 1 Player | 1       		 |
| Start 1 Player | 2       		 |
| Tilt           | Right Shift |

#### Game Controller

| Action         | Key         |
|----------------|-------------|
| Move Left      | D-Pad Left  |
| Move Right     | D-Pad Right |
| Shoot          | A or X      |
| Insert Coin    | Y           |
| Start 1 Player | Start       |
| Start 2 Player | Back        |
| Tilt           | Left Bumper |

### Emulator

|       Action      |   Key(s)  |
|:-----------------:|:---------:|
| Toggle Fullscreen | Alt-Enter |
| Take Screenshot   | Alt-S     |


# References

- Excellent sound samples from https://samples.mameworld.info/Unofficial%20Samples.htm
- Thanks to Tobias V.Langhoff for his article on emulating the look and feel of Space Invaders. I used his findings as a reference. https://tobiasvl.github.io/blog/space-invaders/
- Used the moonshine source code as a reference when writing some of the effects: https://github.com/vrld/moonshine
- https://www.computerarcheology.com/Arcade/SpaceInvaders/Hardware.html
