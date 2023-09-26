# win-nvim-qt-adapter

## What's that?

As some of you may know, `nvim-qt` treats all the command line arguments it receives as the arguments for the Qt frontend. If you want to pass arguments to `nvim` itself, you need to write them after a `--` separator.

This may cause some inconvenience, especially if you want to set `nvim-qt` as the default editor in your operating system.

So to solve this exact problem, you may use this wrapper program, which would forward all the command line arguments it receives to `nvim-qt` with the separator being put right before them.

In other words, `win-nvim-qt-adapter.exe argument1 argument2 ...` is equivalent to `nvim-qt -- argument1 argument2 ...`.

## How does your program know where `nvim-qt` binary is located?

Its path is found in all the folders that are specified in your system's `%PATH%`. If your nvim folder is not there, this adapter won't work.

## How can I use it?

You need to compile it:
```
clang++ -std=c++20 -O3 .\win-nvim-qt-adapter.cpp -lshell32 -lshlwapi -o win-nvim-qt-adapter.exe
```

This program may also be compiled with `cl.exe` from Microsoft, but you should figure it out yourself, because I'm not that familiar with its command line interface.

After you've compiled it, you may use it however you like! You may choose it in "Open with" dialog in Windows' file explorer, or run it using `cmd.exe`.

## Could you make something like this for Linux/macOS?

Sure, here you go:
```sh
#!/bin/sh

nvim-qt -- "$@"
```

Save this script wherever you like, and then run `chmod +x THE_FILENAME_OF_THE_SCRIPT` and you may call it done.

## Why is it a C++ program, rather than `.bat`/`.ps1` script?

If you select `.bat` script in the "Open with" dialog, the `cmd.exe` window will be shown up, and it won't go away until you stop working with `nvim-qt`, which is quite annoying.

`.ps1` script cannot be chosen in "Open with" dialog, so is any other script.


## What are this program's system requirements?

Honestly, I don't know. It works on my Windows 11 machine, but I think you won't be satisfied with this answer.

## It doesn't work on my machine. Can you help me?

Well, I think I can try. Open an issue here or something.

## Can I open a pull request?

Sure.
