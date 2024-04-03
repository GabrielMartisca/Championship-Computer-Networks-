PRAGMA foreign_keys=OFF;
BEGIN TRANSACTION;
CREATE TABLE users (
id INTEGER PRIMARY KEY AUTOINCREMENT,
email TEXT NOT NULL UNIQUE,
password TEXT NOT NULL
, isAdmin INTEGER);
INSERT INTO users VALUES(1,'user1@example.com','parola123',0);
INSERT INTO users VALUES(2,'user2@example.com','parolabuna',0);
INSERT INTO users VALUES(3,'','mama',0);
INSERT INTO users VALUES(4,'gabrielmartisca@yahoo.com','estera',1);
INSERT INTO users VALUES(5,'gabrielmartisca111@gmail.com','turcuman',0);
INSERT INTO users VALUES(6,'andreimoisa@efoarte.inalt','siProst',0);
INSERT INTO users VALUES(7,'filipmartisca@yahoo.com','fcsbesteaua',0);
INSERT INTO users VALUES(8,'mama@mama.mama','mama',0);
INSERT INTO users VALUES(9,'exemplu@yahoo.com','exemplu',0);
INSERT INTO users VALUES(10,'exemplu2@gmail.com','exemplu',0);
INSERT INTO users VALUES(11,'franciscamartisca@gmail.com','mama',NULL);
INSERT INTO users VALUES(12,'andrei.moisa.23@gmail.com','mama',NULL);
INSERT INTO users VALUES(13,'da@da.da','da',NULL);
CREATE TABLE user_championships (
user_id INT,
championship_id INT,
PRIMARY KEY (user_id, championship_id),
FOREIGN KEY (user_id) REFERENCES users(id),
FOREIGN KEY (championship_id) REFERENCES "championships"(id)
);
CREATE TABLE IF NOT EXISTS "championships" (
id INTEGER PRIMARY KEY AUTOINCREMENT,
name VARCHAR(100) NOT NULL,
game VARCHAR(100) NOT NULL,
num_players INT NOT NULL,
rules TEXT,
structure TEXT,
drawing_method TEXT,
accepted_players INT
);
INSERT INTO championships VALUES(1,'Chess Championship','Chess',32,'Chess rules','Single elimination','Random',NULL);
INSERT INTO championships VALUES(2,'Boxing','box',12,'punch face, dont kick','single elimination','rigged',NULL);
INSERT INTO championships VALUES(3,'One Piece','Pirates',10,'Zoro','Binks no sake','Brook',NULL);
INSERT INTO championships VALUES(4,'Hanorac','Checkers',16,'platfus','single elimination','random',NULL);
DELETE FROM sqlite_sequence;
INSERT INTO sqlite_sequence VALUES('users',13);
INSERT INTO sqlite_sequence VALUES('championships',4);
COMMIT;
