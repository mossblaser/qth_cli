Qth Command Line Interface
==========================

This repository contains a commandline interface for interacting with
the [Qth](https://github.com/mossblaser/qth) home automation system.

Usage
-----

The `qth` command has a number of subcommands and a summary of their basic
usage is shown below.

* List the contents of a directory

  `qth --ls [PATH]`

  or to list recursively

  `qth --ls -R [PATH]`

* Show information about a topic
  
  `qth --show TOPIC`

* Set the value of a property

  `qth --set TOPIC VALUE`

  or taking the value from stdin

  `echo VALUE | qth --set TOPIC`

  or setting the value multiple times from stdin

  `(echo VALUE1; echo VALUE2; echo VALUE3) | qth --set TOPIC`
  
  or register the property as a one-to-many property and take values from
  stdin
  
  `qth --set TOPIC --register "DESCRIPTION HERE" [--delete-on-unregister] [--on-unregister=VALUE]`

* Get the current value of a property

  `qth --get -1 TOPIC`

* Watch the value of a property over time

  `qth --get [-c COUNT] TOPIC`
  
  or watch a many-to-one property, registering it 
  
  `qth --get TOPIC --register "DESCRIPTION HERE" [--delete-on-unregister] [--on-unregister=VALUE]`

* Delete a property

  `qth --delete TOPIC`

* Send an event

  `qth --send TOPIC VALUE`

  or taking the value from stdin

  `echo VALUE | qth --send TOPIC`

  or producing multiple events

  `(echo VALUE1; echo VALUE2; echo VALUE3) | qth --send TOPIC`
  
  or register the event as a one-to-many event and take values from stdin
  
  `qth --send TOPIC --register "DESCRIPTION HERE" [--on-unregister=VALUE]`

* Watch an event

  `qth --watch [-c COUNT] TOPIC`

  or just waiting for the first time the event is fired

  `qth --watch -1 TOPIC`

  or register the event as a many-to-one event and watch it
  
  `qth --watch TOPIC --register "DESCRIPTION HERE" [--on-unregister=VALUE]`

In addition, for simple uses where interacting with already-registered topics,
the `qth` command can work out the expected action automatically:

* Getting the current value of a property

  `qth TOPIC`

* Setting the value of a property

  `qth TOPIC VALUE`

* Watching an event

  `qth TOPIC`

* Sending an event

  `qth TOPIC VALUE`
