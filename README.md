# ESP32 Credential Harvester

## Overview

This project uses an ESP32 to deauthenticate users from a target network, then redirects them to a fake access point. Upon connecting, users are prompted to log in with their social media credentials. Even with correct login details, they are sent to a _"Please try again"_ page, prompting them to re-enter their credentials. The attacker can then capture and view the stored login information.

## Credits

- [M1z23R](https://github.com/M1z23R/ESP8266-EvilTwin)
- [jeretc](https://github.com/jeretc/captive-portal)

## Contributing

Feel free to contribute to the project. Pull requests, bug fixes, and improvements are always welcome!
