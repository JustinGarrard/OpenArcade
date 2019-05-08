# OpenArcade

## The Premise
OpenArcade is an effort to accumulate terminal run arcade style games which run without dependencies. That is, all of the games under the OpenArcade heading should be written in such a way as it should always work - across every machine and OS. The easiest way to do this is to write POSIX standard C games!
OpenArcade also aims to provide a stripped down canvas for young hackers to experiment with coding, packaging, and game design in an environment without complexity from dependencies from environments or packages/libraries. We want you to contribute games and game modifications or improvements!

## Contributing
If you create a new game you would like to be included in OpenArcade, please add it by creating a new folder with the game's name, including a Makefile in that folder, and providing game play and startup instructions below (as demonstrated by the GunFight description).

## Getting Started
Clone this repository:
> git clone https://github.com/JustinGarrard/OpenArcade.git

Take a look around:
> cd OpenArcade

> ls

Compile a game:
> cd GunFight

> make

If you want to contribute (new game or edits):
> git branch -b NewBranch

If you make a new game:
> mkdir NewGame

> cd NewGame

Update your changes:
> git push origin NewBranch

To share your new additions with us, create a Pull Request!

## GunFight
Our version of GunFight is based on a popular arcade game by the same name, Gun Fight. Our version is much simpler, but in essence two players attempt to shoot at and dodge each other. To play GunFight you will need two players with an internet connection. 

Start the game like this:
> cd GunFight

> make
Then, one player will start the game as the host by specifying a port and a difficulty:
> ./gunfight --host <port> <difficulty>
For example:
> ./gunfight --host 8080 easy
Then the host must find their public IP address [a fun exercise for new hackers!] and supply it to the other player who joins the game:
> ./gunfight host_ip_address 8080
Note, you must supply the same port as the host who started the game

Now you're off and shooting!

## Where to start tinkering
* Right now, we don't have the game difficulty changing anything about the game come up with a way to alter gameplay based on this parameter
  *Hint, dR and dL change the speed of bullets
* Maybe games should restart after players reach a certain score
* Can you change the characters from hashtags?
* Can you make the game screen bigger?
