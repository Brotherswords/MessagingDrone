#include <string.h>
#include <stdio.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <ctype.h>
#include <math.h>

#define bufferSize 250
#define MAX_TUPLES 100         // Max Tuples per Message
#define MAX_MESSAGES 500       // Max number of messages to store
#define MAX_CONFIG_ENTRIES 100 // Max Number of config entries to store from config file
#define TTLDefault "6"         // Time to live
#define resendTTL 3            // TTL used for resending messages
#define currentVersion 8       // Version of the protocol

// Tuple struct that stores key, value pairs
struct Tuple
{
  char *key;
  char *value;
};

struct messageUID
{
  int seqNumber;
  int fromPort;
  int toPort;
  int budgetTTL;
};

struct configEntry
{
  char ip[20];
  int port;
  char location[5];
  int seqNumber;
  int seqNumberAcked;
  int LACK;
};

// Function that returns a tuple containing the key and value given a string in the format "key:value"
struct Tuple getTuple(char *str)
{
  struct Tuple tuple;
  char *token = strchr(str, ':');

  // If there is no colon -> no key value pair can be extracted -> values set to INVALID
  if (token == NULL)
  {
    tuple.key = "INVALID";
    tuple.value = "INVALID";
    return tuple;
  }

  // If there is a colon -> get index of key
  int keyIndex = token - str;
  // printf("Key Index: %d\n", keyIndex);
  tuple.key = malloc(keyIndex + 1);

  if (tuple.key == NULL)
  {
    printf("Failed to allocate memory\n");
    tuple.key = "INVALID";
  }

  // Copy and add null terminating sequence
  strncpy(tuple.key, str, keyIndex);
  tuple.key[keyIndex] = '\0';

  // Set value to point to the string right after the colon!
  tuple.value = token + 1;
  return tuple;
}

void isNumber(char *str);

void verifyNumArgs(int argc, int expectedArgc);

void inRange(int portNumber);

int verifyPortNumber(struct Tuple *tuples, int portNumber);

struct Tuple *tokenizer(char *str, int *num_structs);

char *messageConversion(char *str, char oldChar, char newChar);

char *messagePreservation(char *str);

char *getLocationFromEntry(struct configEntry entries[], int entryCount, int targetPort);

char *convertTuplesToString(struct Tuple *list, int size);

void appendKeyValueToFront(char *key, char *value, char buffer[]);

void sendEncodedMessage(char *message, struct configEntry entries[], int sd, char *currentLocation, char *sendPath);

int replaceKey(char *key, char *value, struct Tuple *tuples, int numberOfTuples);

int findKey(char *key, struct Tuple *tuples, int numberOfTuples);

char *getKey(char *key, struct Tuple *tuples, int num_structs);

int isMessageInRange(int n, int m, int height, int width, int portNumber, int otherPortNumber);

int TTLProcessing(struct Tuple *tuples, int numberOfTuples);

void printMessage(const char *location, struct Tuple *tuples, int num_structs);

int processMessage(int portValid, int inRange, struct Tuple *tuples, int num_structs);

int setSocketFDs(fd_set *socketFDS, int sd);

void appendPortNumber(char *sendPath, int portnumber);

int getEntryByPort(struct configEntry entries[], int entryCount, int targetPort);

void buildMessage(int seqNumber, int toPort, int fromPort, int location, char *type, char *message);

void appendKeyValueToEnd(char *key, char *value, char buffer[]);

int obtainMove(struct Tuple *tuples, int numberOfTuples);

int find_port(const char *input_string, int portNumber);

int processMessageWithoutPortValid(int inRange, struct Tuple *tuples, int num_structs);

void printAllMessages(const char *location, struct Tuple *listOfTuples[MAX_MESSAGES], int idxNum, int num_structs_count[]);

void deepcopy_and_place(struct Tuple *tuples, int num_structs, struct Tuple *listOfTuples[], int index);

void initialize_message_UIDS(struct messageUID messages[], int size);

int find_next_available_idx(struct messageUID messages[], int size);

void print_messages_array(struct messageUID messages[], int size);

void resend_messages(struct messageUID messages[], int messageUID_size, char *location, struct Tuple *listOfTuples[MAX_MESSAGES], int idxNum, int num_structs_count[], struct configEntry entries[], int sd, int portNumber);

int messageMatch(char *location, struct Tuple *listOfTuples[MAX_MESSAGES], int idxNum, int num_structs_count[], int toPortSearch, int fromPortSearch, int seqNumSearch);

int alreadyExists(int seqNumber, int fromPort, int toPort, int idx, struct messageUID messages[]);

int main(int argc, char *argv[])
{
  int sd;                            // socket descriptor
  struct sockaddr_in server_address; // my address
  struct sockaddr_in from_address;   // address of sender
  char buffer[bufferSize] = {0};     // used in recvfrom
  int portNumber;                    // get this from command line
  int rc = 0;                        // Return code
  socklen_t fromLength;
  int flags = 0;                                        // Parameter for Recvfrom
  struct Tuple *listOfTuples[MAX_MESSAGES] = {0};       // Stores a list of arrays where each array contains a set of key value pairs (in the form of a struct) - its a table
  int num_structsCount[MAX_MESSAGES] = {0};             // Stores the number of structs in each array
  struct configEntry entries[MAX_CONFIG_ENTRIES] = {0}; // Stores a list of config entries - its a table
  struct messageUID messages[MAX_MESSAGES] = {0};       // Stores a list of messageUIDs and resendVals
  int messageNum = 0;
  fd_set socketFDS;
  struct timeval timeout;
  int maxSD;
  int boardWidth;
  int boardHeight;
  int idx;

  // Verify that we have at least the correct number of arguments
  verifyNumArgs(argc, 5);

  initialize_message_UIDS(messages, MAX_MESSAGES);

  // Create the socket
  sd = socket(AF_INET, SOCK_DGRAM, 0);

  if (sd < 0)
  {
    perror("socket error");
    exit(1);
  }
  // Check if the port is a number
  isNumber(argv[1]);

  // Check if the port number is a number
  portNumber = strtol(argv[1], NULL, 10);
  inRange(portNumber);

  // Check if board dimensions are numbers
  isNumber(argv[3]);
  isNumber(argv[4]);
  boardWidth = strtol(argv[3], NULL, 10);
  boardHeight = strtol(argv[4], NULL, 10);

  // Setup server address, portnumber, and for any adapter
  server_address.sin_family = AF_INET;
  server_address.sin_port = htons(portNumber);
  server_address.sin_addr.s_addr = INADDR_ANY;

  // Bind the socket to the port
  rc = bind(sd, (struct sockaddr *)&server_address,
            sizeof(struct sockaddr));

  if (rc < 0)
  {
    perror("binding error");
    exit(1);
  } // clear the buffer

  // TODO: Open config.file, store values for destination, ports, locations and find location matching port
  FILE *file = fopen(argv[2], "r");
  if (file == NULL)
  {
    printf("Error opening file1\n");
    exit(1);
  }

  int entryCount = 0;
  while (fgets(buffer, sizeof(buffer), file) != NULL)
  {
    // Parse the line from the file and store it in the current entry
    sscanf(buffer, "%s %d %s", entries[entryCount].ip, &entries[entryCount].port, entries[entryCount].location);

    // Set the seqNumber and seqNumberAcked fields to 1 for the current entry
    entries[entryCount].seqNumber = 1;
    entries[entryCount].seqNumberAcked = 0;
    entries[entryCount].LACK = -1;

    // Move on to the next entry
    entryCount++;
  }

  // prints IP addresses, Ports, and Locations
  for (int i = 0; i < entryCount; i++)
  {
    printf("IP: %s, Port: %d, Value: %s\n", entries[i].ip, entries[i].port, entries[i].location);
  }

  fclose(file);

  char *location = getLocationFromEntry(entries, entryCount, portNumber);

  if (location == NULL)
  {
    printf("No matching configEntry found.\n");
    exit(1);
  }
  else
  {
    printf("Location: %s\n", location);
  }

  while (1)
  {
    memset(buffer, 0, bufferSize);
    for (int i = 0; i < bufferSize; i++)
    {
      buffer[i] = 0;
    }

    maxSD = setSocketFDs(&socketFDS, sd);

    // TODO - 1: Implement CLI input
    idx = messageNum % MAX_MESSAGES;
    int move_var = -1;
    int cmd_line_input = 0;

    printf("Waiting for input...\n");
    timeout.tv_sec = 20;
    timeout.tv_usec = 0;
    rc = select(maxSD + 1, &socketFDS, NULL, NULL, &timeout);
    if (FD_ISSET(fileno(stdin), &socketFDS))
    {
      fgets(buffer, bufferSize, stdin);
      buffer[strcspn(buffer, "\n")] = 0;
      cmd_line_input = 1;
    }

    if (rc == 0)
    { // had a timeout!
      printf("My fellow Americans, our time has run out, and the urgency of the situation calls for immediate action. Let us swiftly come together and resend those messages,\n");
      resend_messages(messages, MAX_MESSAGES, location, listOfTuples, idx + 1, num_structsCount, entries, sd, portNumber);
      continue;
    }

    if (FD_ISSET(sd, &socketFDS))
    {
      rc = recvfrom(sd, buffer, bufferSize, flags, (struct sockaddr *)&from_address, &fromLength);
    }

    int num_structs;
    struct Tuple *tuples = tokenizer(buffer, &num_structs);

    // TODO: Check if move is real
    int portValid = verifyPortNumber(tuples, portNumber);
    move_var = obtainMove(tuples, num_structs);

    if (move_var != -1)
    {
      // Check if received via the command line -> send message
      if (cmd_line_input == 1)
      {
        char *move_message = convertTuplesToString(tuples, num_structs);
        sendEncodedMessage(move_message, entries, sd, location, "Key Not Found");
      }

      // hehe location is getting replaced if the port matches up
      if (portValid != -1)
      {
        char str[4];
        sprintf(str, "%d", move_var);
        location = str;
        printf("Location Changed to: %s\n", location);
        resend_messages(messages, MAX_MESSAGES, location, listOfTuples, idx + 1, num_structsCount, entries, sd, portNumber);
      }
      continue;
    }

    char *encodedMessage;
    int ttlCheck = TTLProcessing(tuples, num_structs);

    if (ttlCheck == 0)
    {
      printf("Message's TTL is 0 sadge -> throwing away message.\n");
      continue;
    }
    else if (ttlCheck != -1)
    {
      char str[4];
      sprintf(str, "%d", ttlCheck);
      replaceKey("TTL", str, tuples, num_structs);
    }

    int otherPortNumber = -1;
    if (portValid != -1)
    {
      char *localPortNumber = getKey("toPort", tuples, num_structs);
      otherPortNumber = atoi(localPortNumber);
    }

    // Search for myLocation tag -> If found replace it otherwise append.
    int nLocation = strtol(location, NULL, 10);
    int findLocationTag = findKey("location", tuples, num_structs);
    int mLocationNum = 0;
    int inRange = 0;
    if (findLocationTag != -1)
    {
      // Compute Euclidean distance
      char *mLocation = getKey("location", tuples, num_structs);
      mLocationNum = strtol(mLocation, NULL, 10);
      isNumber(mLocation);
      inRange = isMessageInRange(nLocation, mLocationNum, boardHeight, boardWidth, portNumber, otherPortNumber);
    }
    else
    {
      printf("No Location Tag Found");
    }
    // Establish Status Variable
    int status = processMessage(portValid, inRange, tuples, num_structs);
    int findSeqNumber = findKey("seqNumber", tuples, num_structs);
    char *seqNumberLocal;
    char *fromPortLocal = getKey("fromPort", tuples, num_structs);
    char *toPortLocal = getKey("toPort", tuples, num_structs);
    int entryIdx = getEntryByPort(entries, entryCount, atoi(fromPortLocal));
    if (findSeqNumber != -1)
    {
      seqNumberLocal = getKey("seqNumber", tuples, num_structs);
    }

    int storeStatus = processMessageWithoutPortValid(inRange, tuples, num_structs);

    int value = findSeqNumber != -1 ? atoi(seqNumberLocal) : entries[entryIdx].seqNumber;
    int alreadyInArray = alreadyExists(value, atoi(fromPortLocal), atoi(toPortLocal), idx + 1, messages);

    // check if message is already in messages table -> if it is then ignore it
    if (storeStatus == 0 && alreadyInArray != 1)
    {
      deepcopy_and_place(tuples, num_structs, listOfTuples, idx);
      int localIndexMessagesUID = find_next_available_idx(messages, MAX_MESSAGES);
      if (portValid != 0)
      {
        messages[localIndexMessagesUID].seqNumber = value;
        messages[localIndexMessagesUID].fromPort = atoi(fromPortLocal);
        messages[localIndexMessagesUID].toPort = atoi(toPortLocal);
      }
      num_structsCount[idx] = num_structs;
    }

    if (status == 0)
    {
      // Check where the message is coming from -> Port Number, Sequence Number
      int seqNumberNumLocal = findSeqNumber != -1 ? atoi(seqNumberLocal) : entries[entryIdx].seqNumber;

      // Check is messageType is ACK -> Handle Accordingly
      if (findKey("type", tuples, num_structs) != -1)
      {
        // printf("Amon Gustavo\n");
        char *key = getKey("type", tuples, num_structs);
        if (strcmp(key, "ACK") == 0 && seqNumberNumLocal > entries[entryIdx].LACK)
        {
          // entries[entryIdx].seqNumberAcked += 1;
          entries[entryIdx].LACK = seqNumberNumLocal;
          listOfTuples[idx] = tuples;

          printMessage(location, listOfTuples[idx], num_structs);

          continue;
        }
        else
        {
          printf("ACK is a duplicate lmaoooo -> throwing away the ACK >:) \n");
          char *sendPathLocal = getKey("send-path", tuples, num_structs);
          printf("Send Path of Duplicate: %s\n", sendPathLocal);
          continue;
        }
      }

      // printf("seqNumLocal: %d, entries[%d].seqNumber: %d", seqNumberNumLocal, entryIdx, entries[entryIdx].seqNumber);
      if (seqNumberNumLocal > entries[entryIdx].seqNumberAcked)
      {
        entries[entryIdx].seqNumberAcked = seqNumberNumLocal;

        if (status == 0)
        {
          printMessage(location, listOfTuples[idx], num_structs);
        }

        // TODO: Build and Send Acknowledgement -> increment the ackNumber
        char ackMessage[bufferSize] = {0};

        buildMessage(entries[entryIdx].seqNumberAcked, atoi(fromPortLocal), portNumber, nLocation, "ACK", ackMessage);
        sendEncodedMessage(ackMessage, entries, sd, location, "Key Not Found");
        messageNum += 1;
        // entries[entryIdx].seqNumberAcked += 1;
        continue;
      }
      else
      {
        // If the message is coming from the same port and has the same sequence number as the last message
        // received from that port, then the message is a duplicate and should be discarded.

        // If duplicate is in Range -> Print Message
        // if (status == 0)
        // {
        //   printMessage(location, listOfTuples[idx], num_structs);
        // }

        printf("Message is a duplicate XD rofl -> throwing away message.\n");
        char *sendPathLocal = getKey("send-path", tuples, num_structs);
        printf("Send Path of Duplicate: %s\n", sendPathLocal);

        // TODO: Get and print send-path here

        continue;
      }
    }
    else
    {
      // Check if port is valid & if in not in range -> still update seqNumber
    }

    // Replace myLocation or add it
    int findKeyVal = findKey("location", tuples, num_structs);
    if (findKeyVal != -1)
    {
      replaceKey("location", location, tuples, num_structs);
      encodedMessage = convertTuplesToString(tuples, num_structs);
    }
    else
    {
      encodedMessage = convertTuplesToString(tuples, num_structs);
      appendKeyValueToFront("location", location, encodedMessage);
    }

    // TODO: ADD SENDPATH FUNCTIONALITY HERE

    int stoppingDupes = 0;

    int findSendPath = findKey("send-path", tuples, num_structs);
    if (findSendPath != -1)
    {
      // Get current send path value
      char *oldSendPath = getKey("send-path", tuples, num_structs);

      stoppingDupes = find_port(oldSendPath, portNumber);

      appendPortNumber(oldSendPath, portNumber);
      replaceKey("send-path", oldSendPath, tuples, num_structs);
      encodedMessage = convertTuplesToString(tuples, num_structs);
    }
    else
    {
      encodedMessage = convertTuplesToString(tuples, num_structs);
      // Make this appendKeyVakueToEND instead of front because length of the key is getting long af fr fr which is starting to mess up other values! - BRUH MOMENT FR
      appendKeyValueToEnd("send-path", argv[1], encodedMessage);
    }

    // Check if sequence number is present and deal with it as needed

    char *toPort = getKey("toPort", tuples, num_structs);
    otherPortNumber = atoi(toPort);
    entryIdx = getEntryByPort(entries, entryCount, otherPortNumber);
    int seqNumberNew = entries[entryIdx].seqNumber;
    char tempSeqNumber[4];
    sprintf(tempSeqNumber, "%d", seqNumberNew);
    if (findSeqNumber != -1)
    {
      replaceKey("seqNumber", tempSeqNumber, tuples, num_structs);
    }
    else
    {
      appendKeyValueToFront("seqNumber", tempSeqNumber, encodedMessage);
    }

    if (ttlCheck == -1)
    {
      appendKeyValueToFront("TTL", TTLDefault, encodedMessage);
    }

    // If the message is meant for me, no need to forward it.

    entries[entryIdx].seqNumber += 1;

    if (portValid == -1 && stoppingDupes == 0)
    {
      char *sendPathLocal = getKey("send-path", tuples, num_structs);
      // increment entries[sending port number].seqNumber +=1
      sendEncodedMessage(encodedMessage, entries, sd, location, sendPathLocal);
    }

    // tuples = tokenizer(encodedMessage, &num_structs);

    if (status == -1)
    {
      continue;
    }

    messageNum = messageNum + 1;
    memset(buffer, 0, bufferSize);
  }
  return 0;
}

// Converts a preserved message back to normal, given an old character (ususally a carrot) and replacing it with a space again.
char *messageConversion(char *str, char oldChar, char newChar)
{
  for (int i = 0; i < strlen(str); i++)
  {
    // printf("str[%d] = %c\n", i, str[i]);
    if (str[i] == oldChar)
    {
      str[i] = newChar;
    }
  }
  return str;
}

// Preserves message by replacing all spaces between quotations with carrots
char *messagePreservation(char *str)
{
  char *ptr = strchr(str, '\"');
  if (ptr)
  {
    int index = ptr - str;
    index++;
    while (str[index] != '\"' && index < strlen(str))
    {
      if (str[index] == ' ')
      {
        str[index] = '^';
      }
      index++;
    }
  }
  return str;
}

// Tokenize the string using spaces
struct Tuple *tokenizer(char *str, int *num_structs)
{
  char *token;

  char str_copy[bufferSize];
  strcpy(str_copy, str);

  // printf("String before MALLOC: %s\n", str);
  struct Tuple *tuples = (struct Tuple *)malloc(MAX_TUPLES * sizeof(struct Tuple));
  // printf("String after MALLOC : %s\n", str);
  str = messagePreservation(str);
  // printf("String after MALLOC & Preservation : %s\n", str);
  // printf("String Copy %s", str_copy);
  //  counting variable to determine the index of the next space in the array to store things
  int i = 0;
  *num_structs = 0;
  token = strtok(str, " ");

  while (token != NULL)
  {
    token = messageConversion(token, '^', ' ');
    struct Tuple tuple = getTuple(token);
    // Check if the tuple is full
    if (tuple.key != NULL && tuple.value != NULL)
    {
      // Stores tuple in array
      tuples[i] = tuple;
      i++;
      (*num_structs)++;
    }
    token = strtok(NULL, " ");
  }

  return tuples;
}

// Checks if the given string is a number
void isNumber(char *str)
{
  for (int i = 0; i < strlen(str); i++)
  {
    if (!isdigit(str[i]))
    {
      perror("At least one input that was meant to be a number - was not a number.");
      exit(1);
    }
  }
}

// Checks if the current port number is within range of all ports
void inRange(int portNumber)
{
  if ((portNumber > 65535) || (portNumber < 0))
  {
    printf("you entered an invalid socket number\n");
    exit(1);
  }
}

// Verifies that the given ip address is an acceptable address
void validIPAddress(char *str)
{
  struct sockaddr_in inaddr;
  if (!inet_pton(AF_INET, str, &inaddr))
  {
    printf("error, bad ip address\n");
    exit(1);
  }
}

// Check if the number of arguments is at least the desired number of arguments
void verifyNumArgs(int argc, int expectedArgc)
{
  if (argc < expectedArgc)
  {
    printf("usage is: server <portnumber> <config.file> <numrows> <numcols>\n");
    exit(1);
  }
}

// Make sure only messages destined for this port are printed
int verifyPortNumber(struct Tuple *tuples, int portNumber)
{
  for (int i = 0; i < MAX_TUPLES; i++)
  {
    if (tuples[i].key != NULL && strcmp(tuples[i].key, "toPort") == 0)
    {
      if (atoi(tuples[i].value) == portNumber)
      {
        return 0;
      }
      else
      {
        return -1;
      }
    }
  }
  return -1;
}

// Searches configEntry array and returns the location for a matching port.
char *getLocationFromEntry(struct configEntry entries[], int entryCount, int targetPort)
{
  for (int i = 0; i < entryCount; i++)
  {
    if (entries[i].port == targetPort)
    {
      return entries[i].location;
    }
  }
  return NULL;
}

// Converts Tuple to a string in the format of a message.
char *convertTuplesToString(struct Tuple *list, int size)
{
  int i, len = 0;
  for (i = 0; i < size; i++)
  {
    if (list[i].key != NULL)
    {
      len += strlen(list[i].key) + strlen(list[i].value) + 3;
    }
    else
    {
      break;
    }
  }

  char *result = malloc(len * sizeof(char));
  if (result == NULL)
  {
    return NULL;
  }

  strcpy(result, "");
  for (i = 0; i < size; i++)
  {
    if (list[i].key != NULL)
    {
      strcat(result, list[i].key);
      strcat(result, ":");
      strcat(result, list[i].value);
      strcat(result, " ");
    }
    else
    {
      break;
    }
  }
  return result;
}

void appendKeyValueToFront(char *key, char *value, char buffer[])
{
  char temp[bufferSize];
  int keyValueLength = snprintf(temp, bufferSize, "%s:%s ", key, value);

  // Shift the original buffer to the right to make room for the key-value pair thing
  for (int i = bufferSize - 1; i >= keyValueLength; i--)
  {
    buffer[i] = buffer[i - keyValueLength];
  }

  // Copy the key-value string into the front of the buffer
  for (int i = 0; i < keyValueLength; i++)
  {
    buffer[i] = temp[i];
  }
}

// Consider adding a parameter for messageLocation to get the location of the message and compare with the location in the entries table to not send a message back.
void sendEncodedMessage(char *message, struct configEntry entries[], int sd, char *currentLocation, char *sendPath)
{
  // Cycle through the list of configEntry objects
  for (int i = 0; i < MAX_CONFIG_ENTRIES; i++)
  {
    struct configEntry entry = entries[i];

    // Check if the location is not equal to currentLocation (don't want to send it to ourselves)
    int found = find_port(sendPath, entry.port);

    // Check if already sent to this port num
    if (strcmp(entry.location, currentLocation) != 0 && found == 0 && entry.ip[0] != '\0')
    {
      struct sockaddr_in server_address; // my address
      server_address.sin_family = AF_INET;
      server_address.sin_port = htons(entry.port);
      server_address.sin_addr.s_addr = inet_addr(entry.ip);

      int rc = 0;
      rc = sendto(sd, message, strlen(message), 0, (struct sockaddr *)&server_address, sizeof(server_address));
      // printf("Sent To! %s %d\n", entry.ip, entry.port);
      if (rc < strlen(message))
      {
        perror("Not all data sent.");
        exit(1);
      }
    }
  }
}

char *getKey(char *key, struct Tuple *tuples, int num_structs)
{
  for (int i = 0; i < num_structs; i++)
  {
    if (tuples[i].key != NULL && strcmp(tuples[i].key, key) == 0)
    {
      return tuples[i].value;
    }
  }
  return "Key Not Found";
}

int findKey(char *key, struct Tuple *tuples, int numberOfTuples)
{
  if (key == NULL)
  {
    return -1;
  }
  for (int i = 0; i < numberOfTuples; i++)
  {
    // if (!tuples[i].key)
    // {
    //   return -1;
    // }
    if (strcmp(tuples[i].key, key) == 0)
    {
      return 0;
    }
  }
  return -1;
}

int replaceKey(char *key, char *value, struct Tuple *tuples, int numberOfTuples)
{
  for (int i = 0; i < numberOfTuples; i++)
  {
    if (!tuples[i].key)
    {
      return -1;
    }
    if (tuples[i].key && strcmp(tuples[i].key, key) == 0)
    {
      tuples[i].value = value;
      return 0;
    }
  }
  return -1;
}

int isMessageInRange(int n, int m, int height, int width, int portNumber, int otherPortNumber)
{
  double distance;
  // Prints board

  // printf("\nBoard:\n");
  // for (int i = 1; i <= width; i++) {
  //   for (int j = 1; j <= height; j++) {
  //     if ((i - 1) * height + j == n || (i - 1) * height + j == m) {
  //       printf("%3s ", "x");
  //     } else {
  //       printf("%3d ", (i - 1) * height + j);
  //     }
  //   }
  //   printf("\n");
  // }
  // printf("\n");

  // Check if message is out of grid
  if (portNumber == otherPortNumber)
  {
    // printf("Message meant for me :D | ");
  }
  else
  {
    // printf("Message not meant for me D:| ");
  }

  if (n < 1 || n > height * width)
  {
    // printf("NOT IN GRID - Message location (%d) is outside the grid\n", m);
    return 1;
  }
  if (m < 1 || m > height * width)
  {
    // printf("NOT IN GRID - Message location (%d) is outside the grid\n", m);
    return 1;
  }
  // Calculate Euclidean distance between n and m

  else
  {
    int xcorN = (n - 1) / height;
    int ycorN = (n - 1) % height;

    int xcorM = (m - 1) / height;
    int ycorM = (m - 1) % height;

    // printf("Distance between (%d, %d) and (%d, %d) - ", xcorN, ycorN, xcorM, ycorM);

    // distance = sqrt(pow((n / height + 1) - (m / height + 1), 2) + pow((n % height + 1) - (m % height + 1), 2));
    distance = sqrt(pow(xcorN - xcorM, 2) + pow(ycorN - ycorM, 2));
    // Check if message is out of range
    if (floor(distance) > 2)
    {
      // printf("OUT OF RANGE - Message is %f units away from current location -> Truncated to %f\n", distance, floor(distance));
      return 2;
    }
    // Message is in range
    else
    {
      // printf("IN RANGE - Message is %f units away from current location -> Truncated to %f\n", distance, floor(distance));
      return 3;
    }
  }

  return -1;
}

int TTLProcessing(struct Tuple *tuples, int numberOfTuples)
{
  int ttl;

  for (int i = 0; i < numberOfTuples; i++)
  {

    // TODO Seartch for TTL

    // Verify TTL exists
    int keyTag = findKey("TTL", tuples, numberOfTuples);
    if (keyTag == -1)
    {
      printf("ttl not in message - time to add it\n");
      return -1;
    }

    // Get TTL and convert it to an integer
    char *ttlValue = getKey("TTL", tuples, numberOfTuples);
    ttl = atoi(ttlValue);

    // Check if its 0 -> Get ready to discard
    if (ttl <= 0)
    {
      // printf("TTL value is 0\n");
      return 0;
    }

    ttl -= 1;
  }
  return ttl;
}

void printMessage(const char *location, struct Tuple *tuples, int num_structs)
{
  printf("%s: %s\n", "This-Drones-Location", location);
  printf("%20s%20s\n", "Key", "Value");
  for (int i = 0; i < num_structs; i++)
  {
    if (tuples[i].key != NULL)
    {
      printf("%20s%20s\n", tuples[i].key, tuples[i].value);
    }
    else
    {
      break;
    }
  }
}

int processMessage(int portValid, int inRange, struct Tuple *tuples, int num_structs)
{
  char *version = getKey("version", tuples, num_structs);
  int issue = 0;
  if (portValid == -1)
  {
    printf("My fellow Americans, we have encountered a minor setback. It appears that a message has been received, but unfortunately it was not intended for this port.\n");
    issue = -1;
  }

  if (inRange != 3)
  {
    printf("Well, let me be clear, folks. The message we have received appears to be, shall we say, out of range.\n");
    issue = -1;
  }

  double versionDouble = strcmp(version, "Key Not Found") != 0 ? atof(version) : -1;

  if (versionDouble < currentVersion)
  {
    printf("Received a message with an outdated version\n");
    issue = -1;
  }

  if (issue == -1)
  {
    return -1;
  }
  else
  {
    return 0;
  }
}

int setSocketFDs(fd_set *socketFDS, int sd)
{
  FD_ZERO(socketFDS);
  FD_SET(sd, socketFDS);
  FD_SET(fileno(stdin), socketFDS);
  if (fileno(stdin) > sd)
  {
    return fileno(stdin);
  }
  else
  {
    return sd;
  }
}

// ISSUE HERE: portstr is overriding things that it shouldnt due to size.
void appendPortNumber(char *sendPath, int portnumber)
{
  char portStr[5]; // 7 is length of ",<port>"
  sprintf(portStr, ",%d", portnumber);
  strcat(sendPath, portStr);
}

int getEntryByPort(struct configEntry entries[], int entryCount, int targetPort)
{
  for (int i = 0; i < entryCount; i++)
  {
    if (entries[i].port == targetPort)
    {
      return i;
    }
  }
  printf("Port Number not in config file\n");
  return -1; // Indicates that no matching entry was found
}

void buildMessage(int seqNumber, int toPort, int fromPort, int location, char *type, char *message)
{
  // Construct the message string in the format "key:value"
  sprintf(message, "seqNumber:%d toPort:%d fromPort:%d location:%d type:%s TTL:%s version:%d send-path:%d", seqNumber, toPort, fromPort, location, type, TTLDefault, currentVersion, fromPort);
}

void appendKeyValueToEnd(char *key, char *value, char buffer[])
{
  char temp[bufferSize];
  int keyValueLength = snprintf(temp, bufferSize, "%s:%s ", key, value);
  if (keyValueLength < 0)
  {
    printf("Certified bruh moment ngl fr fr pain\n");
  }

  // Append the key-value string to the end of the buffer
  strcat(buffer, temp);
}

int obtainMove(struct Tuple *tuples, int numberOfTuples)
{

  // Verify TTL exists
  int keyTag = findKey("move", tuples, numberOfTuples);
  if (keyTag == -1)
  {
    // printf("no move :D treating like a normal message\n");
    return -1;
  }

  // Get move and convert it to an integer
  char *move = getKey("move", tuples, numberOfTuples);
  int moveLoc = atoi(move);

  return moveLoc;
}

int find_port(const char *input_string, int portNumber)
{
  if (strcmp(input_string, "Key Not Found") == 0)
  {
    return 0;
  }

  int found = 0;
  int current_number = 0;

  for (int i = 0; input_string[i] != '\0'; i++)
  {
    if (input_string[i] >= '0' && input_string[i] <= '9')
    {
      current_number = current_number * 10 + (input_string[i] - '0');
    }
    else if (input_string[i] == ',')
    {
      if (current_number == portNumber)
      {
        found = 1;
        break;
      }
      current_number = 0;
    }
  }

  // Checking the last number in the list
  if (current_number == portNumber)
  {
    found = 1;
  }

  return found;
}

int processMessageWithoutPortValid(int inRange, struct Tuple *tuples, int num_structs)
{
  char *version = getKey("version", tuples, num_structs);
  int issue = 0;

  if (inRange != 3)
  {
    printf("Well, let me be clear, folks. The message we have received appears to be, shall we say, out of range.\n");
    issue = -1;
  }

  double versionDouble = strcmp(version, "Key Not Found") != 0 ? atof(version) : -1;

  if (versionDouble < currentVersion)
  {
    printf("Received a message with an outdated version\n");
    issue = -1;
  }

  if (issue == -1)
  {
    return -1;
  }
  else
  {
    return 0;
  }
}

// Write a function to print everything in the tuple double array
void printAllMessages(const char *location, struct Tuple *listOfTuples[MAX_MESSAGES], int idxNum, int num_structs_count[])
{
  for (int i = 0; i < idxNum; i++)
  {
    if (listOfTuples[i] != NULL)
    {
      printMessage(location, listOfTuples[i], num_structs_count[i]);
    }
  }
}

void deepcopy_and_place(struct Tuple *tuples, int num_structs, struct Tuple *listOfTuples[], int index)
{
  struct Tuple *new_tuples = (struct Tuple *)malloc(num_structs * sizeof(struct Tuple));
  for (int i = 0; i < num_structs; i++)
  {
    new_tuples[i].key = strdup(tuples[i].key);
    new_tuples[i].value = strdup(tuples[i].value);
  }
  listOfTuples[index] = new_tuples;
}

void initialize_message_UIDS(struct messageUID messages[], int size)
{
  for (int i = 0; i < size; i++)
  {
    messages[i].seqNumber = -1;
    messages[i].fromPort = -1;
    messages[i].toPort = -1;
    messages[i].budgetTTL = resendTTL;
  }
}

int find_next_available_idx(struct messageUID messages[], int size)
{
  for (int i = 0; i < size; i++)
  {
    if (messages[i].seqNumber == -1 || messages[i].budgetTTL <= 0)
    {
      return i;
    }
  }
  return -1; // Return -1 when no such spaces exist, as 0 is a valid index.
}
void print_messages_array(struct messageUID messages[], int size)
{
  for (int i = 0; i < size; i++)
  {
    if (messages[i].seqNumber != -1)
    {
      printf("Message %d: seqNumber=%d, fromPort=%d, toPort=%d, budgetTTL=%d\n",
             i, messages[i].seqNumber, messages[i].fromPort, messages[i].toPort, messages[i].budgetTTL);
    }
  }
}

void resend_messages(struct messageUID messages[], int messageUID_size, char *location, struct Tuple *listOfTuples[MAX_MESSAGES], int idxNum, int num_structs_count[], struct configEntry entries[], int sd, int portNumber)
{
  // print_messages_array(messages, messageUID_size);
  for (int i = 0; i < messageUID_size; i++)
  {
    if (messages[i].seqNumber != -1 && messages[i].budgetTTL > 0)
    {
      messages[i].budgetTTL -= 1;
      // find matching message in listOfTuples
      int matching_idx = messageMatch(location, listOfTuples, idxNum, num_structs_count, messages[i].toPort, messages[i].fromPort, messages[i].seqNumber);
      if (matching_idx != -1)
      {
        replaceKey("location", location, listOfTuples[matching_idx], num_structs_count[matching_idx]);
        char *encodedMessage = convertTuplesToString(listOfTuples[matching_idx], num_structs_count[matching_idx]);
        char *sendPath = getKey("send-path", listOfTuples[matching_idx], num_structs_count[matching_idx]);
        int stoppingDupes = find_port(sendPath, portNumber);
        int localToPort = atoi(getKey("toPort", listOfTuples[matching_idx], num_structs_count[matching_idx]));
        if (stoppingDupes == 0 && localToPort != portNumber)
        {
          // printf("Sending encoded message: %s", encodedMessage);
          sendEncodedMessage(encodedMessage, entries, sd, location, sendPath);
        }
      }
    }
  }
}

int messageMatch(char *location, struct Tuple *listOfTuples[MAX_MESSAGES], int idxNum, int num_structs_count[], int toPortSearch, int fromPortSearch, int seqNumSearch)
{
  for (int i = 0; i < idxNum; i++)
  {
    int local_num_structs = num_structs_count[i];
    char *toPortLocal = getKey("toPort", listOfTuples[i], local_num_structs);
    char *fromPortLocal = getKey("fromPort", listOfTuples[i], local_num_structs);
    char *seqNumLocal = getKey("seqNumber", listOfTuples[i], local_num_structs);

    if (atoi(toPortLocal) == toPortSearch && atoi(fromPortLocal) == fromPortSearch && atoi(seqNumLocal) == seqNumSearch)
    {
      return i;
    }
  }
  return -1;
}

int alreadyExists(int seqNumber, int fromPort, int toPort, int idx, struct messageUID messages[])
{
  for (int i = 0; i < MAX_MESSAGES; i++)
  {
    // printf("Comparing %d, %d, %d to %d, %d, %d", messages[i].seqNumber, messages[i].fromPort, messages[i].toPort, seqNumber, fromPort, toPort);
    if (messages[i].seqNumber == seqNumber && messages[i].fromPort == fromPort && messages[i].toPort == toPort)
    {
      return 1;
    }
  }
  return 0;
}