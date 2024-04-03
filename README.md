# Championships Management System

The Championships Management System simplifies the organization of championships for various sports or games. Users can easily register and participate, while organizers customize tournament details. Notably, our participant management system sends automated email notifications regarding acceptance status. The system also offers efficient match scheduling and administrative tools. Whether for local events or international competitions, our platform supports organizers and participants alike.## Key Features

- **User Registration and Authentication**: Users can easily register accounts and securely log in to access the system's functionalities.
- **Championship Registration**: Organizers can create and manage championships, specifying essential details such as game rules, tournament structure, and scheduling preferences.
- **Participant Management**: Players and teams can register to participate in championships, facilitating seamless communication and coordination. When a user tries to join a championship, an email is sent to inform them if they have been accepted or not.
- **Match Scheduling and Management**: The system offers tools for scheduling matches, updating match statuses, and handling rescheduling requests efficiently.
- **Administrative Tools**: Administrators have access to additional functionalities for overseeing championships, managing participants, and ensuring smooth operations.


## User Commands

1. **Login**: `login <username>` - Log in with a specified username.
2. **Register**: `register` - Register a new user.
3. **Get Championships**: `get championships` - Retrieve a list of available championships.
4. **Join Championship**: `join <championship_id>` - Join/participate in a specific championship.
5. **Make Admin**: `make admin <username>` - Promote a user to administrator status.
6. **Update Winner**: `update winner <match_id> <winner>` - Update the winner of a match.
7. **Get Results**: `get results <championship_id>` - Retrieve the results of matches in a specific championship.
8. **Postpone Match**: `postpone <match_id>` - Postpone/reschedule a match by one hour.
9. **Logout**: `exit` - Log out from the current session.

## Usage

### Compiling the Server and Client Files

- **Compile Server.c**:

  ```bash
  gcc server.c -o server -lpthread -lsqlite3
  
This command compiles server.c into an executable named server with support for POSIX threads (-lpthread flag) and linking with SQLite3 library (-lsqlite3 flag).

- **Compile Client.c**:
  
  ```bash
  gcc client.c -o client
  
This command compiles client.c into an executable named client.

- **Running the Server and Client**:
  
Run the server by executing the compiled binary:

  ```bash
  ./server

This starts the server and listens for incoming connections.
Start the Client:

Open another terminal window/tab.
Run the client by executing its compiled binary:

  ```bash
  ./client

This starts the client application, and it will connect to the server.
