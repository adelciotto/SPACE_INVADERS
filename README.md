# space_invaders

A Space Invaders arcade emulator written in C++. Still a WIP!

<img src="/preview.png" width="752" height="646">

# Build from source

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

TODO!

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
