Qth Command Line Interface
==========================

This repository contains a commandline interface for interacting with
the [Qth](https://github.com/mossblaser/qth) home automation system.

When provided with a topic, the tool tries to guess the right thing to do
    
    $ qth lounge/light true
    $ qth lounge/temperature
    18.5
    $ qth kitchen/bell
    $ qth kitchen/movement
    null
    null
    ^C

Tab completion is also available

    $ qth lou<TAB>
    $ qth lounge/

A listing command is also provided

    $ qth ls
    meta/
    lounge/
    kitchen/
    $ qth ls kitchen/
    light
    bell
    movement

You can also be explicit about what you want to do (and the command will fail
if the topic has the wrong behaviour)

    $ qth set lounge/light true
    $ qth send lounge/light true
    Error: Topic does not have behaviour 'EVENT-N:1'.

But you can always override this checking...

    $ qth set lounge/light true
    $ qth get lounge/light
    Error: Topic does not have behaviour 'PROPERTY-1:N'.
    $ qth get --force lounge/light
    true

You can also register new topics for testing purposes

    $ qth watch --register lounge/tv/power
    true
    false
    false
    ^C

Try '--help' for a complete list of supported features.

Compilation and Installation
----------------------------

Build and install with:

    $ make
    $ sudo make install

