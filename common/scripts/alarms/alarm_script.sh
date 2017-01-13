#!/bin/bash

# Enter python virtual environment.
pyenv

# Invoke the alarm handler.
python alarm_handler.py $1 $2

# Exit python virtual environment.
deactivate 