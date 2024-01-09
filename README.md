CS 214 Project 3: My shell <br>
Names: Akash Shah and Teerth Patel <br>
NetIDs: ass209 and tp554 <br>

Main idea: <br> 
The main purpose of this project was to create a program that is able to properly act as a shell, interpreting commands in 2 different modes: batch and interactive. It will be able to do anything a simple command-line shell can do, except handling more than 2 processes (for simplicity). <br>

Test Plans <br>
The main goal with our test plan is to try and push our custom shell to its limits, each test done will be executed to test each part of our shell design to make sure it is robust and through, and can withstand edge cases. All these tests were executed in batch and interactive mode. <br> 

The first test suite is described below: <br>
The first test is one that makes sure that the program can properly interpret simple commands (like pwd, cd, echo, etc.), without piping, and give the correct output. If these simple commands are missing required arguments, we return an appropriate error and exit the program. We properly search the directories /usr/local/bin, /usr/bin, and /bin to properly run our commands with the full path to the executable. What we are trying to do here is make sure that the program can parse through each token properly, and can run execv() properly. <br>


Our second test suite is described below: <br>
We tested commands with just input redirection, output redirection, and both. We tested redirecting output to file which does not exist, which would mean the program would have to create that file. We also tried inputting from a file which does not exist, which throws the appropriate error. In addition, we tried commands that had both the input and output redirection (e.g: sort < input.txt > output_sorted.txt), ensuring that our program can handle all cases. In addition, we also tried inputting/ouputting to/from wildcards, ensuring that whatever file matches the pattern that the command gives is included in the redirection.  <br>

<br>

Our third test suite is described below: <br>
The third test is one that works on testing the ability of the program to properly communicate between processes. Simple commands with pipes will be tested (such as cat Hello! > file1.txt | echo to ensure Hello! is outputted) for functionality. <br>

With this the program has a large amount of travel distance from the very last directory to the very first. Within the time it takes it to get to the first test1, it would make stops to several different text files that it would need to properly read and count before finally getting back to the first directory and printing the results. Successfully running this test would prove that our program is fully capable of parsing a multitude of directories that vary in size, along with the fact that it can be the necessary stops to count and text files in the way.<br>

