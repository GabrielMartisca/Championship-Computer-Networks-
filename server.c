#define _XOPEN_SOURCE 700
#include <unistd.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sqlite3.h>
#include <time.h>
#include <pthread.h>

#define PORT 8081
#define MAX_CONNECTIONS 10
#define MAX_MESSAGE_SIZE 1024
#define QUERY_BUFFER_SIZE 8192

typedef enum
{
    CHAMP_IDLE,
    ADD_ID,
    ADD_NAME,
    ADD_GAME,
    ADD_MAXPLAYERS,
    ADD_RULES,
    ADD_STRUCTURE,
    ADD_DRAWING_METHOD,
    ADD_DATE,

} AddChampionshipState;

typedef enum
{
    STATE_IDLE,
    STATE_REGISTER_EMAIL,
    STATE_REGISTER_PASSWORD,
    STATE_REGISTER_CONFIRM_PASSWORD,
    STATE_REGISTER_COMPLETED,
} RegistrationState;

typedef enum
{
    LSTATE_IDLE,
    LSTATE_ENTER_EMAIL,
    LSTATE_ENTER_PASS,
    LSTATE_CHECK_PASS,
} LoginState;

enum
{
    COMMAND_UNKNOWN = 0,
    COMMAND_1,
    COMMAND_2,
    COMMAND_3,
    COMMAND_4,
    COMMAND_5,
    COMMAND_6,
    COMMAND_7,
    COMMAND_8,
    COMMAND_9,
    COMMAND_10,
};

struct oneChamp
{
    int id;
    char name[MAX_MESSAGE_SIZE];
    char game[MAX_MESSAGE_SIZE];
    char rules[MAX_MESSAGE_SIZE];
    char structure[MAX_MESSAGE_SIZE];
    char drawMethod[MAX_MESSAGE_SIZE];
    int maxPlayers;
    char date[MAX_MESSAGE_SIZE];
};

char details[1024];

int convertCommandToEnum(const char *command)
{
    if (strncmp(command, "login ", 5) == 0)
    {
        return COMMAND_1;
    }
    else if (strncmp(command, "register", 8) == 0)
    {
        return COMMAND_2;
    }
    else if (strncmp(command, "add championship", 16) == 0)
    {
        return COMMAND_3;
    }
    else if (strcmp(command, "logout") == 0)
    {
        return COMMAND_4;
    }
    else if (strncmp(command, "get championships", 17) == 0)
    {
        return COMMAND_5;
    }
    else if (strncmp(command, "join ", 4) == 0)
    {
        return COMMAND_6;
    }
    else if (strncmp(command, "make admin ", 11) == 0)
    {
        return COMMAND_7;
    }
    else if (strncmp(command, "update winner ", 14) == 0)
    {
        return COMMAND_8;
    }
    else if (strncmp(command, "get results ", 12) == 0)
    {
        return COMMAND_9;
    }
    else if (strncmp(command, "postpone ", 9) == 0)
    {
        return COMMAND_10;
    }
    else
        return COMMAND_UNKNOWN;
}

sqlite3 *openDatabase()
{
    sqlite3 *db;
    if (sqlite3_open("mydatabase.db", &db) != SQLITE_OK)
    {
        fprintf(stderr, "Cannot open database: %s\n", sqlite3_errmsg(db));
        exit(EXIT_FAILURE);
    }
    return db;
}

void closeDatabase(sqlite3 *db)
{
    sqlite3_close(db);
}

int isEmailInDatabase(sqlite3 *db, const char *email)
{
    sqlite3_stmt *stmt;
    const char *query = "SELECT COUNT(*) FROM users WHERE email = ?;";
    int result = -1;

    if (sqlite3_prepare_v2(db, query, -1, &stmt, 0) == SQLITE_OK)
    {
        sqlite3_bind_text(stmt, 1, email, -1, SQLITE_STATIC);
        if (sqlite3_step(stmt) == SQLITE_ROW)
        {
            result = sqlite3_column_int(stmt, 0);
        }
        sqlite3_finalize(stmt);
    }

    return result;
}

int isValidEmail(const char *email)
{
    // Find the position of the '@' character
    const char *atSymbol = strchr(email, '@');

    // Find the position of the last '.' character
    const char *lastDot = strrchr(email, '.');

    // Check if '@' and '.' are present and in the correct order
    if (atSymbol != NULL && lastDot != NULL && atSymbol < lastDot)
    {
        // Check if there is only one '@' symbol
        if (strchr(atSymbol + 1, '@') == NULL)
        {
            // Check if there are no consecutive dots
            const char *dot = atSymbol;
            while ((dot = strchr(dot + 1, '.')) != NULL)
            {
                if (dot[1] == '.')
                {
                    return 0; // Multiple consecutive dots
                }
            }

            // Check if the email ends with a dot
            if (lastDot[1] != '\0')
            {
                return 1; // Email matches the pattern
            }
        }
    }

    return 0; // Email does not match the pattern
}

int getAcceptedPlayersCount(sqlite3 *db, int championshipId)
{
    sqlite3_stmt *stmt;
    const char *query = "SELECT COUNT(*) FROM user_championships WHERE championship_id = ?;";
    int count = 0;

    if (sqlite3_prepare_v2(db, query, -1, &stmt, 0) == SQLITE_OK)
    {
        sqlite3_bind_int(stmt, 1, championshipId);

        if (sqlite3_step(stmt) == SQLITE_ROW)
        {
            count = sqlite3_column_int(stmt, 0);
        }

        sqlite3_finalize(stmt);
    }

    return count;
}

int getMaxPlayers(sqlite3 *db, int championshipId)
{
    sqlite3_stmt *stmt;
    const char *query = "SELECT num_players FROM championships WHERE id = ?;";
    int maxPlayers = 0;

    if (sqlite3_prepare_v2(db, query, -1, &stmt, 0) == SQLITE_OK)
    {
        sqlite3_bind_int(stmt, 1, championshipId);

        if (sqlite3_step(stmt) == SQLITE_ROW)
        {
            maxPlayers = sqlite3_column_int(stmt, 0);
        }

        sqlite3_finalize(stmt);
    }

    return maxPlayers;
}

void incrementAcceptedPlayers(sqlite3 *db, int championshipId)
{
    const char *query = "UPDATE championships SET accepted_players = accepted_players + 1 WHERE id = ?;";
    sqlite3_stmt *stmt;

    if (sqlite3_prepare_v2(db, query, -1, &stmt, 0) == SQLITE_OK)
    {
        sqlite3_bind_int(stmt, 1, championshipId);

        if (sqlite3_step(stmt) != SQLITE_DONE)
        {
            fprintf(stderr, "Error updating accepted players count.\n");
        }

        // Finalize the statement
        sqlite3_finalize(stmt);
    }
    else
    {
        fprintf(stderr, "Error preparing query to update accepted players count.\n");
    }
}

void promoteToAdmin(sqlite3 *db, int clientID)
{
    char query[QUERY_BUFFER_SIZE];

    snprintf(query, sizeof(query), "UPDATE users SET isAdmin = 1 WHERE id = %d;", clientID);

    if (sqlite3_exec(db, query, 0, 0, 0) != SQLITE_OK)
    {
        fprintf(stderr, "Error updating isAdmin in the database: %s\n", sqlite3_errmsg(db));
    }
}

int findGamesWithUser1(sqlite3 *db)
{
    char query[512];

    snprintf(query, sizeof(query), "SELECT id FROM games WHERE user1_id IS NOT NULL AND user2_id =-1;");

    sqlite3_stmt *stmt;
    int gameId = -1;

    if (sqlite3_prepare_v2(db, query, -1, &stmt, 0) == SQLITE_OK)
    {
        if (sqlite3_step(stmt) == SQLITE_ROW)
        {
            gameId = sqlite3_column_int(stmt, 0);
        }

        sqlite3_finalize(stmt);
    }
    printf("game id: %d\n", gameId);
    return gameId;
}

int executeQuery(sqlite3 *db, const char *query)
{
    sqlite3_stmt *stmt = NULL;

    if (sqlite3_prepare_v2(db, query, -1, &stmt, NULL) != SQLITE_OK)
    {
        fprintf(stderr, "Error preparing SQL statement: %s\n", sqlite3_errmsg(db));
        return -1;
    }

    if (sqlite3_step(stmt) != SQLITE_DONE)
    {
        fprintf(stderr, "Error executing SQL statement: %s\n", sqlite3_errmsg(db));
        sqlite3_finalize(stmt);
        return -1;
    }

    sqlite3_finalize(stmt);

    return 0;
}

void sendAcceptedEmail(sqlite3 *db, const char *recipientEmail, char details[])
{
    char command[1024];
    snprintf(command, sizeof(command), "sendemail -f noreplychampionship@gmail.com -t %s -u 'Update on your join request' -m 'Congratulations! You have been accepted to join the championship! The match will take place on:%s' -s smtp.gmail.com:587 -o tls=yes -xu noreplychampionship@gmail.com -xp 'dnvs pkje hrgy vpsn'", recipientEmail, details);

    system(command);
    printf("sent accepted mail\n");
}

void sendDeclinedEmail(sqlite3 *db, const char *recipientEmail, int error)
{
    char command[1024];
    if (error == 2)
    {
        snprintf(command, sizeof(command), "sendemail -f noreplychampionship@gmail.com -t %s -u 'Update on your join request' -m 'Unfortunately, it seems we could not accept your joing request. The championship is full. But keep surfing the championship list and try another one!' -s smtp.gmail.com:587 -o tls=yes -xu noreplychampionship@gmail.com -xp 'dnvs pkje hrgy vpsn'", recipientEmail);
    }
    else
    {
        snprintf(command, sizeof(command), "sendemail -f noreplychampionship@gmail.com -t %s -u 'Update on your join request' -m 'Unfortunately, it seems we could not accept your joing request. You already joined this championship. But keep surfing the championship list and try another one!' -s smtp.gmail.com:587 -o tls=yes -xu noreplychampionship@gmail.com -xp 'dnvs pkje hrgy vpsn'", recipientEmail);
    }
    system(command);
    printf("sent declined mail\n");
}

int getUserIdByEmail(sqlite3 *db, const char *email)
{
    char query[256];
    snprintf(query, sizeof(query), "SELECT id FROM users WHERE email = '%s';", email);

    sqlite3_stmt *stmt;
    int userId = -1;

    if (sqlite3_prepare_v2(db, query, -1, &stmt, 0) == SQLITE_OK)
    {
        if (sqlite3_step(stmt) == SQLITE_ROW)
        {
            userId = sqlite3_column_int(stmt, 0);
        }

        sqlite3_finalize(stmt);
    }

    return userId;
}

const char *getLastGameDatetime(sqlite3 *db, int championshipId)
{
    // Build the query to get the last game datetime
    char query[4048];
    snprintf(query, sizeof(query), "SELECT strftime('%%Y-%%m-%%d %%H:%%M:%%S', date) FROM games WHERE championship_id = %d ORDER BY id DESC LIMIT 1;", championshipId);

    // Execute the query and retrieve the result
    sqlite3_stmt *stmt;
    const char *lastGameDatetime = NULL;
    if (sqlite3_prepare_v2(db, query, -1, &stmt, 0) == SQLITE_OK)
    {
        if (sqlite3_step(stmt) == SQLITE_ROW)
        {
            lastGameDatetime = strdup((const char *)sqlite3_column_text(stmt, 0));
            printf("Last game time inside the function:%s\n", lastGameDatetime);
        }
        sqlite3_finalize(stmt);
    }

    return lastGameDatetime;
}

int isLeapYear(int year)
{
    return (year % 4 == 0 && (year % 100 != 0 || year % 400 == 0));
}

int daysInMonth(int year, int month)
{
    const int daysPerMonth[] = {0, 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    if (month == 2 && isLeapYear(year))
    {
        return 29;
    }
    return daysPerMonth[month];
}

int createNewGame(sqlite3 *db, int championshipId, int userId)
{
    char query[2048];
    sqlite3_stmt *stmt;

    // Get the last game datetime for the championship
    const char *lastGameDatetime = getLastGameDatetime(db, championshipId);
    printf("Last game time: %s\n", lastGameDatetime);

    // If there is no existing game, use the date from the championships table
    if (lastGameDatetime == NULL)
    {
        snprintf(query, sizeof(query), "SELECT strftime('%%Y-%%m-%%d %%H:%%M:%%S', date) FROM championships WHERE id = %d;", championshipId);

        if (sqlite3_prepare_v2(db, query, -1, &stmt, 0) == SQLITE_OK)
        {
            if (sqlite3_step(stmt) == SQLITE_ROW)
            {
                lastGameDatetime = strdup((const char *)sqlite3_column_text(stmt, 0));
                printf("Last game time got from championship: %s\n", lastGameDatetime);
            }
            sqlite3_finalize(stmt);
        }
    }

    // Check if a valid lastGameDatetime was obtained
    if (lastGameDatetime != NULL)
    {
        char x[2048];
        strcpy(x, lastGameDatetime);
        printf("x:%s\n", x);

        int day, month, year, hour;

        // Extract values using sscanf
        sscanf(x, "%d-%d-%d %d:", &year, &month, &day, &hour);
        printf("day: %d, month: %d, year: %d, hour: %d\n", day, month, year, hour);

        hour += 1;

        if (hour >= 24)
        {
            hour = 0;

            day += 1;

            if (day > daysInMonth(year, month))
            {
                day = 1;

                month += 1;

                if (month > 12)
                {
                    month = 1;

                    year += 1;
                }
            }
        }

        // Format the updated date and time
        snprintf(x, sizeof(x), "%04d-%02d-%02d %02d:%s", year, month, day, hour, x + 14);

        printf("Updated x: %s\n", x);
        strcpy(details, x);
        // Insert a new game into the games table with the user as user1 and the updated datetime
        char insertQuery[4048];
        snprintf(insertQuery, sizeof(insertQuery), "INSERT INTO games (user1_id, user2_id, championship_id, date, winner_id, postponed) VALUES (%d, -1, %d, '%s', -1, 0);",
                 userId, championshipId, x);

        if (executeQuery(db, insertQuery) != -1)
        {
            return 0;
        }
    }

    return -1;
}

int updateExistingGame(sqlite3 *db, int gameId, int userId)
{
    char query[512];

    // Update the existing game with the second user
    snprintf(query, sizeof(query), "UPDATE games SET user2_id = %d WHERE id = %d;", userId, gameId);
    int result = executeQuery(db, query);

    if (result != -1)
    {
        printf("Game %d updated with User %d as user2.\n", gameId, userId);
    }
    else
    {
        printf("Failed to update game %d with User %d.\n", gameId, userId);
    }

    return result;
}

void handleJoinRequest(sqlite3 *db, const char *receivedMessage, const char *email, char *resp)
{
    char champName[MAX_MESSAGE_SIZE], champGame[MAX_MESSAGE_SIZE], rules[MAX_MESSAGE_SIZE], structure[MAX_MESSAGE_SIZE], drawMethod[MAX_MESSAGE_SIZE];
    int championshipId = 1;
    int denied = 0;
    sscanf(receivedMessage, "join %d", &championshipId); 

    int userId = getUserIdByEmail(db, email); 
    char query[512];

    // Check if the user is already in the championship
    snprintf(query, sizeof(query), "SELECT COUNT(*) FROM games WHERE championship_id = %d AND (user1_id = %d OR user2_id = %d);", championshipId, userId, userId);

    sqlite3_stmt *stmt;

    if (sqlite3_prepare_v2(db, query, -1, &stmt, 0) == SQLITE_OK)
    {
        if (sqlite3_step(stmt) == SQLITE_ROW)
        {
            int count = sqlite3_column_int(stmt, 0);
            printf("count:%d\n", count);
            if (count > 0)
            {
                sendDeclinedEmail(db, email, 1);
                strcpy(resp, "Check your email for updates on your join request!");
                denied = 1;
            }
        }

        sqlite3_finalize(stmt);
    }
    if (denied != 1)
    {

        // Check if the championship is full
        snprintf(query, sizeof(query), "SELECT accepted_players, num_players FROM championships WHERE id = %d;", championshipId);
        int result;
        if (sqlite3_prepare_v2(db, query, -1, &stmt, 0) == SQLITE_OK)
        {
            if (sqlite3_step(stmt) == SQLITE_ROW)
            {
                int acceptedPlayers = sqlite3_column_int(stmt, 0);
                int maxPlayers = sqlite3_column_int(stmt, 1);

                if (acceptedPlayers >= maxPlayers)
                {
                     sendDeclinedEmail(db, email, 2);
                    strcpy(resp, "Check your email for updates on your join request!!");
                }
                else
                {
                    int game = findGamesWithUser1(db);
                    if (game != -1)
                    {
                        result = updateExistingGame(db, game, userId); 
                    }
                    else
                    {
                        result = createNewGame(db, championshipId, userId); 
                    }
                    if (result == 0)
                    {
                        printf("Join request handled successfully!\n");
                        incrementAcceptedPlayers(db, championshipId);
                        strcpy(resp, "Check your email for updates on your join request.");
                        sendAcceptedEmail(db, email, details);
                    }
                    else
                    {
                        printf("Join request failed!\n");
                    }
                }
            }

            sqlite3_finalize(stmt);
        }
    }
}

int isAdminUser(sqlite3 *db, const char *email)
{
    sqlite3_stmt *stmt;
    const char *query = "SELECT isAdmin FROM users WHERE email = ?;";
    int isAdmin = -1;

    if (sqlite3_prepare_v2(db, query, -1, &stmt, 0) == SQLITE_OK)
    {
        sqlite3_bind_text(stmt, 1, email, -1, SQLITE_STATIC);

        if (sqlite3_step(stmt) == SQLITE_ROW)
        {
            isAdmin = sqlite3_column_int(stmt, 0);
        }

        sqlite3_finalize(stmt);
    }

    return isAdmin;
}

int authenticateUser(sqlite3 *db, const char *email, const char *password)
{
    sqlite3_stmt *stmt;
    const char *query = "SELECT COUNT(*) FROM users WHERE email = ? AND password = ?;";
    int result = -1;

    if (sqlite3_prepare_v2(db, query, -1, &stmt, 0) == SQLITE_OK)
    {
        sqlite3_bind_text(stmt, 1, email, -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 2, password, -1, SQLITE_STATIC);
        if (sqlite3_step(stmt) == SQLITE_ROW)
        {
            result = sqlite3_column_int(stmt, 0);
        }
        sqlite3_finalize(stmt);
    }

    return result;
}

int registerUser(sqlite3 *db, const char *email, const char *password)
{
    sqlite3_stmt *stmt;
    const char *query = "INSERT INTO users (email, password) VALUES (?, ?);";
    int result = -1;

    if (sqlite3_prepare_v2(db, query, -1, &stmt, 0) == SQLITE_OK)
    {
        sqlite3_bind_text(stmt, 1, email, -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 2, password, -1, SQLITE_STATIC);
        if (sqlite3_step(stmt) == SQLITE_DONE)
        {
            result = 0; // Registration successful
        }
        sqlite3_finalize(stmt);
    }

    return result;
}

void insertChampionship(sqlite3 *db, struct oneChamp *championship)
{
    char query[QUERY_BUFFER_SIZE];

    snprintf(query, sizeof(query), "INSERT INTO championships (name, game, num_players, rules, structure, drawing_method, accepted_players, date) VALUES ('%s', '%s', %d, '%s', '%s', '%s', 0, datetime('%s'));",
             championship->name, championship->game, championship->maxPlayers,
             championship->rules, championship->structure, championship->drawMethod, championship->date);

    if (sqlite3_exec(db, query, 0, 0, 0) != SQLITE_OK)
    {
        fprintf(stderr, "Error inserting championship into the database: %s\n", sqlite3_errmsg(db));
    }
    else
    {
        printf("Championship successfully inserted into the database.\n");
    }
}

void fetchChampionships(sqlite3 *db, char *result)
{
    sqlite3_stmt *stmt;
    const char *query = "SELECT * FROM championships;";

    strcat(result, "(");

    if (sqlite3_prepare_v2(db, query, -1, &stmt, 0) == SQLITE_OK)
    {
        while (sqlite3_step(stmt) == SQLITE_ROW)
        {
            struct oneChamp champ;

            // Get values from the current row
            champ.id = sqlite3_column_int(stmt, 0);
            snprintf(champ.name, sizeof(champ.name), "%s", sqlite3_column_text(stmt, 1));
            snprintf(champ.game, sizeof(champ.game), "%s", sqlite3_column_text(stmt, 2));
            champ.maxPlayers = sqlite3_column_int(stmt, 3);
            snprintf(champ.rules, sizeof(champ.rules), "%s", sqlite3_column_text(stmt, 4));
            snprintf(champ.structure, sizeof(champ.structure), "%s", sqlite3_column_text(stmt, 5));
            snprintf(champ.drawMethod, sizeof(champ.drawMethod), "%s", sqlite3_column_text(stmt, 6));

            strcat(result, "{");
            strcat(result, "\"id\":");
            char idStr[MAX_MESSAGE_SIZE];
            snprintf(idStr, sizeof(idStr), "%d", champ.id);
            strcat(result, idStr);
            strcat(result, ",\"name\":\"");
            strcat(result, champ.name);
            strcat(result, "\",\"game\":\"");
            strcat(result, champ.game);
            strcat(result, "\",\"num_players\":");
            char numPlayersStr[MAX_MESSAGE_SIZE];
            snprintf(numPlayersStr, sizeof(numPlayersStr), "%d", champ.maxPlayers);
            strcat(result, numPlayersStr);
            strcat(result, ",\"rules\":\"");
            strcat(result, champ.rules);
            strcat(result, "\",\"structure\":\"");
            strcat(result, champ.structure);
            strcat(result, "\",\"drawing_method\":\"");
            strcat(result, champ.drawMethod);
            strcat(result, "\"}\n");
        }

        if (strlen(result) > 1)
        {
            result[strlen(result) - 1] = '\0';
        }

        strcat(result, ")");

        sqlite3_finalize(stmt);
    }
    else
    {
        fprintf(stderr, "Error preparing SQL statement: %s\n", sqlite3_errmsg(db));
    }
}

int isValidDate(char *date)
{
    int year, month, day, hour, minute, second;

    if (sscanf(date, "%d-%d-%d %d:%d:%d", &year, &month, &day, &hour, &minute, &second) != 6)
    {
        return 0; 
    }

    // Check if the values are within valid ranges
    if (year < 0 || month < 1 || day < 1 || hour < 0 || minute < 0 || second < 0 ||
        year > 9999 || month > 12 || hour > 23 || minute > 59 || second > 59)
    {
        return 0; // Invalid
    }

    const int daysInMonth[] = {0, 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};

    if (month == 2)
    {
        // Check for leap year
        if ((year % 4 == 0 && year % 100 != 0) || (year % 400 == 0))
        {
            if (day > 29)
            {
                return 0; 
            }
        }
        else
        {
            if (day > 28)
            {
                return 0; 
            }
        }
    }
    else
    {
        if (day > daysInMonth[month])
        {
            return 0; 
        }
    }

    time_t currentTime;
    struct tm *currentLocalTime;
    currentTime = time(NULL);
    currentLocalTime = localtime(&currentTime);

    // Check if the date is in the past
    if (year < currentLocalTime->tm_year + 1900 ||
        (year == currentLocalTime->tm_year + 1900 && month < currentLocalTime->tm_mon + 1) ||
        (year == currentLocalTime->tm_year + 1900 && month == currentLocalTime->tm_mon + 1 && day < currentLocalTime->tm_mday) ||
        (year == currentLocalTime->tm_year + 1900 && month == currentLocalTime->tm_mon + 1 && day == currentLocalTime->tm_mday &&
         hour < currentLocalTime->tm_hour) ||
        (year == currentLocalTime->tm_year + 1900 && month == currentLocalTime->tm_mon + 1 && day == currentLocalTime->tm_mday &&
         hour == currentLocalTime->tm_hour && minute < currentLocalTime->tm_min) ||
        (year == currentLocalTime->tm_year + 1900 && month == currentLocalTime->tm_mon + 1 && day == currentLocalTime->tm_mday &&
         hour == currentLocalTime->tm_hour && minute == currentLocalTime->tm_min && second < currentLocalTime->tm_sec))
    {
        return 0; // Date is in the past
    }

    // The date is valid
    return 1;
}

int postponeMatch(sqlite3 *db, int clientId, int gameId)
{
    char query[256];
    sqlite3_stmt *stmt;

    // Check if the client is part of the game
    snprintf(query, sizeof(query), "SELECT 1 FROM games WHERE (user1_id = ? OR user2_id = ?) AND id = ?;");
    if (sqlite3_prepare_v2(db, query, -1, &stmt, 0) == SQLITE_OK)
    {
        sqlite3_bind_int(stmt, 1, clientId);
        sqlite3_bind_int(stmt, 2, clientId);
        sqlite3_bind_int(stmt, 3, gameId);

        if (sqlite3_step(stmt) == SQLITE_ROW)
        {
            // Client is part of the game
            snprintf(query, sizeof(query), "UPDATE games SET date = datetime(date, '+1 hour') WHERE id = ?;");
            sqlite3_finalize(stmt);

            if (sqlite3_prepare_v2(db, query, -1, &stmt, 0) == SQLITE_OK)
            {
                sqlite3_bind_int(stmt, 1, gameId);

                if (sqlite3_step(stmt) != SQLITE_DONE)
                {
                    fprintf(stderr, "Error updating match time: %s\n", sqlite3_errmsg(db));
                }
                else
                {
                    printf("Match postponed by 1 hour.\n");
                    return 1;
                }
            }
            else
            {
                fprintf(stderr, "Error preparing update query: %s\n", sqlite3_errmsg(db));
            }
        }
        else
        {
            printf("Client is not part of the specified game.\n");
            return 0;
        }

        // Finalize the statement
        sqlite3_finalize(stmt);
    }
    else
    {
        fprintf(stderr, "Error preparing query: %s\n", sqlite3_errmsg(db));
    }
}

void printChampionshipResults(sqlite3 *db, char *response, int championshipId)
{
    char query[QUERY_BUFFER_SIZE];
    sqlite3_stmt *stmt;

    snprintf(query, sizeof(query),
             "SELECT g.id, u1.email AS user1, u2.email AS user2, g.winner_id "
             "FROM games g "
             "JOIN users u1 ON g.user1_id = u1.id "
             "JOIN users u2 ON g.user2_id = u2.id "
             "WHERE g.championship_id = %d;",
             championshipId);

    if (sqlite3_prepare_v2(db, query, -1, &stmt, 0) == SQLITE_OK)
    {
        int allGamesHaveWinner = 1; 

        strcpy(response, "");

        while (sqlite3_step(stmt) == SQLITE_ROW)
        {
            int gameId = sqlite3_column_int(stmt, 0);
            const char *user1 = (const char *)sqlite3_column_text(stmt, 1);
            const char *user2 = (const char *)sqlite3_column_text(stmt, 2);
            int winnerId = sqlite3_column_int(stmt, 3);

            if (winnerId == -1)
            {
                allGamesHaveWinner = 0; // Not all games have winner
                break;                 
            }


            char gameInfo[128];
            snprintf(gameInfo, sizeof(gameInfo), "Game ID: %d, User1: %s, User2: %s, Winner: %d\n", gameId, user1, user2, winnerId);
            strcat(response, gameInfo);
        }

        sqlite3_finalize(stmt);

        printf("allgameshavewinner: %d\n", allGamesHaveWinner);
        if (!allGamesHaveWinner)
        {
            strcpy(response, "Not all games have a winner.");
        }
    }
    else
    {
        strcpy(response, "Error retrieving championship results.");
    }
}

void updateGameWinner(sqlite3 *db, int gameId, int winner)
{
    char query[QUERY_BUFFER_SIZE];

    snprintf(query, sizeof(query), "UPDATE games SET winner_id = %d WHERE id = %d;", winner, gameId);

    if (sqlite3_exec(db, query, 0, 0, 0) != SQLITE_OK)
    {
        fprintf(stderr, "Error updating winner in the games database: %s\n", sqlite3_errmsg(db));
    }
}

int isGameIdInDatabase(sqlite3 *db, int gameId)
{
    char query[QUERY_BUFFER_SIZE];
    sqlite3_stmt *stmt;

    snprintf(query, sizeof(query), "SELECT 1 FROM games WHERE id = %d LIMIT 1;", gameId);

    if (sqlite3_prepare_v2(db, query, -1, &stmt, 0) == SQLITE_OK)
    {
        int result = sqlite3_step(stmt);

        if (result == SQLITE_ROW)
        {
            sqlite3_finalize(stmt);
            return 1; // Game ID exists
        }
    }

    sqlite3_finalize(stmt);

    return 0; // Game ID doesn't exist or err
}

void *handleClient(void *arg)
{
    sqlite3 *db = openDatabase();
    int clientSocket = *((int *)arg);
    int commandNumber;
    int loggedIn = 0;
    char receivedMessage[MAX_MESSAGE_SIZE], response[MAX_MESSAGE_SIZE];
    RegistrationState registrationState = STATE_IDLE;
    LoginState loginState = LSTATE_IDLE;
    AddChampionshipState addChampState = CHAMP_IDLE;
    char userEmail[MAX_MESSAGE_SIZE];
    char userPassword[MAX_MESSAGE_SIZE];
    struct oneChamp champ;
    int registerProccessState = 0;
    int idReq;
    while (1)
    {
        // Receive message from client
        ssize_t bytesReceived = recv(clientSocket, receivedMessage, sizeof(receivedMessage) - 1, 0);
        if (bytesReceived <= 0)
        {
            if (bytesReceived == 0)
            {
                // Connection closed by the client
                printf("Client disconnected\n");
            }
            else
            {
                perror("Error in recv");
            }
            close(clientSocket);
            pthread_exit(NULL);
        }
        receivedMessage[bytesReceived] = '\0'; // Add null terminator
        //Handle message
        if (registerProccessState == 0)
        {
            commandNumber = convertCommandToEnum(receivedMessage);
            printf("commandNumber:%d\n", commandNumber);
        }
        else if (registerProccessState == 2)
            commandNumber = COMMAND_2;
        else if (registerProccessState == 1)
            commandNumber = COMMAND_1;
        else if (registerProccessState == 3)
            commandNumber = COMMAND_3;
        switch (commandNumber)
        {
        case COMMAND_1:
            if (loggedIn == 0)
            {
                switch (loginState)
                {
                case LSTATE_IDLE:
                    strcpy(response, "Enter your email:");
                    registerProccessState = 1;
                    loginState = LSTATE_ENTER_EMAIL;
                    break;
                case LSTATE_ENTER_EMAIL:
                    if (!isValidEmail(receivedMessage))
                    {
                        strcpy(response, "Invalid email! Enter a valid email:");
                        loginState = LSTATE_ENTER_EMAIL;
                        break;
                    }
                    else
                    {
                        if (!isEmailInDatabase(db, receivedMessage))
                        {
                            strcpy(response, "The email you entered isn't linked to an account! Find your email and try again:");
                            loginState = LSTATE_ENTER_EMAIL;
                            break;
                        }
                        else
                        {
                            strcpy(response, "Enter your password: ");
                            strcpy(userEmail, receivedMessage);
                            loginState = LSTATE_ENTER_PASS;
                            break;
                        }
                    }
                case LSTATE_ENTER_PASS:
                    printf("Entered entering password state\n");
                    if (!authenticateUser(db, userEmail, receivedMessage))
                    {
                        printf("password and email do not match\n");
                        strcpy(response, "Incorrect password! Try again:");
                        loginState = LSTATE_ENTER_PASS;
                        break;
                    }
                    else
                    {
                        printf("pass and email match\n");
                        strcpy(response, "Login successful!");
                        registerProccessState = 0;
                        loginState = LSTATE_IDLE;
                        // sendAcceptedEmail(userEmail);
                        // sendDeniedEmail(userEmail);
                        loggedIn = 1;
                    }
                }
            }
            else
                strcpy(response, "Already logged in!");
            break;
        case COMMAND_2:
            if (loggedIn == 0)
            {
                switch (registrationState)
                {
                case STATE_IDLE:
                    registerProccessState = 2;
                    strcpy(response, "Enter an email:");
                    registrationState = STATE_REGISTER_EMAIL;
                    break;
                case STATE_REGISTER_EMAIL:
                    // check for email validity and stuff
                    if (!isValidEmail(receivedMessage))
                    {
                        strcpy(response, "Invalid email! Enter a valid email:");
                        registrationState = STATE_REGISTER_EMAIL;
                        break;
                    }
                    else
                    {
                        if (isEmailInDatabase(db, receivedMessage))
                        {
                            strcpy(response, "Email already in use! Enter a different email:");
                            registrationState = STATE_REGISTER_EMAIL;
                            break;
                        }
                        else
                        {
                            strcpy(response, "Enter a password: ");
                            strcpy(userEmail, receivedMessage);
                            registrationState = STATE_REGISTER_PASSWORD;
                            break;
                        }
                    }
                case STATE_REGISTER_PASSWORD:
                    strcpy(userPassword, receivedMessage);
                    strcpy(response, "Confirm your password: ");
                    registrationState = STATE_REGISTER_CONFIRM_PASSWORD;
                    break;
                case STATE_REGISTER_CONFIRM_PASSWORD:
                    if (strcmp(userPassword, receivedMessage) == 0)
                    {
                        strcpy(response, "Registration complete!");
                        registerProccessState = 0;
                        printf("User email: %s\n", userEmail);
                        printf("Password: %s\n", userPassword);
                        registerUser(db, userEmail, userPassword); // adding the user to the database
                        registrationState = STATE_IDLE;
                    }
                    else
                    {
                        strcpy(response, "Passwords do not match. Enter a password:");
                        registrationState = STATE_REGISTER_PASSWORD;
                    }
                    break;
                }
            }
            else
                strcpy(response, "You are already logged in!");
            break;
        case COMMAND_3:
            if (loggedIn == 1)
            {
                if (isAdminUser(db, userEmail))
                {
                    switch (addChampState)
                    {
                    case CHAMP_IDLE:
                        strcpy(response, "Enter championship name: ");
                        registerProccessState = 3;
                        addChampState = ADD_NAME;
                        break;
                    case ADD_NAME:
                        strcpy(champ.name, receivedMessage);
                        strcpy(response, "Enter game name: ");
                        addChampState = ADD_GAME;
                        break;
                    case ADD_GAME:
                        strcpy(champ.game, receivedMessage);
                        strcpy(response, "Enter maximum number of players:");
                        addChampState = ADD_MAXPLAYERS;
                        break;
                    case ADD_MAXPLAYERS:
                        champ.maxPlayers = atoi(receivedMessage);
                        strcpy(response, "Enter rules: ");
                        addChampState = ADD_RULES;
                        break;
                    case ADD_RULES:
                        strcpy(champ.rules, receivedMessage);
                        strcpy(response, "Enter structure: ");
                        addChampState = ADD_STRUCTURE;
                        break;
                    case ADD_STRUCTURE:
                        strcpy(champ.structure, receivedMessage);
                        strcpy(response, "Enter drawing method: ");
                        addChampState = ADD_DRAWING_METHOD;
                        break;
                    case ADD_DRAWING_METHOD:
                        strcpy(champ.drawMethod, receivedMessage);
                        strcpy(response, "Enter date and time, e.g. : yyyy-mm-dd hh:mm:ss ");
                        addChampState = ADD_DATE;
                        break;
                    case ADD_DATE:
                        int isDateCorrect = isValidDate(receivedMessage);
                        if (isDateCorrect)
                        {
                            strcpy(champ.date, receivedMessage);
                            strcpy(response, "Championship added!");
                            insertChampionship(db, &champ);
                            addChampState = CHAMP_IDLE;
                            registerProccessState = 0;
                        }
                        else
                        {
                            strcpy(response, "Invalid date! Try again!(yyyy-mm-dd hh:mm:ss)");
                            addChampState = ADD_DATE;
                        }
                    }
                }
                else
                    strcpy(response, "You need to be an admin to add championships!");
            }
            else
                strcpy(response, "You need to be logged in!");
            break;
        case COMMAND_4:
            if (loggedIn == 1)
            {
                strcpy(response, "Logget out successfully!");
                loggedIn = 0;
                memset(userEmail, 0, sizeof(userEmail));
                memset(userPassword, 0, sizeof(userPassword));
            }
            else
                strcpy(response, "You are not logged in!");
            break;
        case COMMAND_5:
            if (loggedIn)
            {
                memset(response, 0, sizeof(response));
                fetchChampionships(db, response);
            }
            else
                strcpy(response, "You are not logged in!");
            break;
        case COMMAND_6:
            if (loggedIn == 1)
            {
                sscanf(receivedMessage, "join %d", &idReq);
                // strcpy(response, "You joined the championship");
                //  actual join implementation
                handleJoinRequest(db, receivedMessage, userEmail, response);
            }
            else
                strcpy(response, "You are not logged in!");
            break;
        case COMMAND_7:
            if (loggedIn == 1)
            {
                if (isAdminUser(db, userEmail))
                {
                    sscanf(receivedMessage, "make admin %d", &idReq);
                    promoteToAdmin(db, idReq);
                    strcpy(response, "User promoted to admin.");
                }
                else
                    strcpy(response, "You need to be an admin!");
            }
            else
                strcpy(response, "You are not logged in!");
            break;
        case COMMAND_8:
            if (loggedIn == 1)
            {
                if (isAdminUser(db, userEmail))
                {
                    int win;
                    sscanf(receivedMessage, "update winner %d,%d", &idReq, &win);
                    if ((win == 1 || win == 2) && isGameIdInDatabase(db, idReq))
                    {
                        updateGameWinner(db, idReq, win);
                        strcpy(response, "Winner updated.");
                    }
                    else
                        strcpy(response, "Update failed.");
                }
                else
                    strcpy(response, "You need to be an admin!");
            }
            else
                strcpy(response, "You are not logged in!");
            break;
        case COMMAND_9:
            if (loggedIn == 1)
            {
                if (isAdminUser(db, userEmail))
                {
                    sscanf(receivedMessage, "get results %d", &idReq);
                    printChampionshipResults(db, response, idReq);
                }
                else
                    strcpy(response, "You need to be an admin!");
            }
            else
                strcpy(response, "You are not logged in!");
            break;
        case COMMAND_10:
            if (loggedIn == 1)
            {
                int clId = getUserIdByEmail(db, userEmail);
                sscanf(receivedMessage, "postpone %d", &idReq);
                int postponed = postponeMatch(db, clId, idReq);
                if (postponed == 1)
                {

                    strcpy(response, "Match postponed!");
                }
                else
                    strcpy(response, "User not part of specified game!");
            }
            else
                strcpy(response, "You need to be logged in!");
            break;
        case COMMAND_UNKNOWN:
            strcpy(response, "invalid command");
            break;
        }
        ssize_t bytesSent = send(clientSocket, response, strlen(response), 0);
        if (bytesSent == -1)
        {
            perror("Error in send");
        }
        memset(receivedMessage, 0, sizeof(receivedMessage));
    }

    close(clientSocket);
    pthread_exit(NULL);
    closeDatabase(db);
}

int main()
{
    int serverSocket, newSocket;
    struct sockaddr_in serverAddr, clientAddr;
    socklen_t addrSize = sizeof(struct sockaddr_in);
    pthread_t thread;

    // Create a socket
    serverSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (serverSocket == -1)
    {
        perror("Socket creation failed");
        exit(EXIT_FAILURE);
    }

    // Set up server address structure
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_addr.s_addr = INADDR_ANY;
    serverAddr.sin_port = htons(PORT);

    int reuse = 1;
    if (setsockopt(serverSocket, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse)) == -1)
    {
        perror("Setting SO_REUSEADDR failed");
        exit(EXIT_FAILURE);
    }

    // Bind the socket to the specified address and port
    if (bind(serverSocket, (struct sockaddr *)&serverAddr, sizeof(serverAddr)) == -1)
    {
        perror("Bind failed");
        exit(EXIT_FAILURE);
    }

    // Listen for incoming connections
    if (listen(serverSocket, MAX_CONNECTIONS) == -1)
    {
        perror("Listen failed");
        exit(EXIT_FAILURE);
    }

    printf("Server listening on port %d...\n", PORT);

    // Accept connections and handle requests
    while (1)
    {
        // Create a new socket for each connection
        newSocket = accept(serverSocket, (struct sockaddr *)&clientAddr, &addrSize);
        if (newSocket == -1)
        {
            perror("Accept failed");
            exit(EXIT_FAILURE);
        }

        // Create a new thread to handle the client
        if (pthread_create(&thread, NULL, handleClient, (void *)&newSocket) != 0)
        {
            perror("Thread creation failed");
            close(newSocket);
        }
        else
        {
            pthread_detach(thread); // Detach the thread to avoid memory leaks
        }

        
    }

    close(serverSocket);
    return 0;
}
