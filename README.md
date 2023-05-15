# Drone Version 8.0

This is a drone-like program written in C. It listens on a specified UDP port for incoming messages and processes them. The code reads in a configuration file at runtime which specifies the destination IP addresses, ports, and locations and sends out messages from the command line to those destination IP addresses and corresponding ports.

## Main Function

1. Creates a UDP socket and binds it to a specified port
2. Reads in a configuration file containing destination IP addresses, ports, and locations
3. Uses select to wait for input from the network and the user
4. Processes incoming messages and stores them in a table, if a message contains a "move" field it is a command-and-control message and as such will change the destination drone's location regardless of what it was before and the remainder of the message will be ignored. Command-and-control messages are not stored in the table nor forwarded to other drones.
5. Sends outgoing messages to all IP addresses specified in the configuration file
6. Prints out the key-value pairs in each message received if TTL > 0, the message is in range and toPort matches the current Drones port
7. Sends an ACK Message back to the sender.

## Constants

- bufferSize: the maximum number of characters that can be read in from a single message
- MAX_TUPLES: the maximum number of tuples allowed per message
- MAX_MESSAGES: the maximum number of messages that can be stored at any given time
- MAX_CONFIG_ENTRIES: the maximum number of config entries to store from the config file
  locations
- TTL: the maximum amount of hops a message can hop, appended if no TTL provided in a message
- resendTTL: the TTL used for resending messages, only resending them when moved or if a timeout occurs
- currentVersion: the current version of the drone, if a message doesnt have a version field or the version is less than currentVersion the message will be ignored.

## Functions

The following are fucntions and their corrresponding descriptions:

- getTuple: extracts a key-value pair from a string in the format "key:value"
- isNumber: checks if a given string is a number
- verifyNumArgs: checks if the correct number of command-line arguments were provided
- inRange: checks if the given port number is within the valid range (0-65535)
- validIPAddress: checks if the given IP address is valid
- verifyPortNumber: checks if the given message is destined for the current port number
- tokenizer: tokenizes a message and returns a list of Tuples
- messagePreservation: preserves a message by replacing all spaces between quotation marks with carrots
- messageConversion: converts a preserved message back to normal, given an old character (usually a carrot) and replacing it with a space again
- searchEntries: searches a list of configEntry structs for an entry with a matching port and returns its location
- convertTuplesToString: converts a list of tuples into a single string message
- appendKeyValueToFront: adds a key-value pair to the beginning of a message
- replaceKey: replaces the value of a key in a list of tuples
- findKey: searches a list of tuples for a given key and returns its index
- getKey: searches a list of tuples for a given key and returns its value
- sendEncodedMessage: sends a message to all IP addresses specified in the configuration file
- isMessageInRange: - This function takes in the coordinates of two points (n and m), the height and width of a grid, and two port numbers. It calculates the Euclidean distance between n and m and checks if the message is within range. It returns 1 if the message location is outside the grid, 2 if the message is out of range, and 3 if the message is in range.
- TTLProcessing: - This function takes in an array of tuples and the number of tuples in the array. It searches for the TTL key in the tuples and returns its value as an integer. It returns -1 if the TTL key is not found, 0 if the TTL value is 0, or the new TTL value after decrementing the old value.
- printMessage: - This function takes in a location string and an array of tuples. It prints out the location and a table of tuples.
- processMessage: - This function takes in a valid port number, a version string, an integer indicating whether the message is in range or not, and an array of tuples. It checks if the message meets the required criteria and prints out error messages if the conditions are not met. If there is an issue, the function frees the memory allocated to the tuples and returns -1. Otherwise, it returns 0.
- setSocketFDs: - This function takes in a pointer to an fd_set structure and a socket descriptor. It initializes the fd_set to zero and sets the socket descriptor and standard input file descriptor to the set. The function also returns the maximum descriptor value.

#### Note: Free() is not used in this program because the program in theory may use malloc() allocated messages indefinitely.
