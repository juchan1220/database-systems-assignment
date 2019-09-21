# 1
SELECT name FROM Pokemon WHERE type = 'Grass' ORDER BY name;

# 2
SELECT name FROM Trainer WHERE hometown = 'Brown City' or hometown = 'Rainbow City' ORDER BY name;

# 3
SELECT DISTINCT type FROM Pokemon ORDER BY type;

# 4
SELECT name FROM City WHERE name LIKE "B%" ORDER BY name;

# 5
SELECT hometown FROM Trainer WHERE name NOT LIKE "M%" ORDER BY hometown;

# 6
SELECT nickname FROM CatchedPokemon WHERE level = (SELECT MAX(level) FROM CatchedPokemon) ORDER BY nickname;

# 7 
SELECT name FROM Pokemon WHERE name REGEXP "^[AEIOUaeiou]" ORDER BY name;

# 8
SELECT AVG(level) FROM CatchedPokemon;

# 9
SELECt MAX(level) FROM CatchedPokemon WHERE owner_id = (SELECT id FROM Trainer WHERE name = "Yellow");

# 10
SELECT DISTINCT hometown FROM Trainer ORDER BY hometown;

# 11
SELECT Trainer.name, CatchedPokemon.nickname FROM Trainer, CatchedPokemon WHERE CatchedPokemon.owner_id = Trainer.id AND CatchedPokemon.nickname LIKE "A%" ORDER BY Trainer.name;

# 12
SELECT Trainer.name FROM Gym JOIN Trainer ON Gym.leader_id = Trainer.id WHERE city IN (SELECT name FROM City WHERE description = "Amazon");

# 13
SELECT Trainer.id, tmp.cnt
FROM Trainer
JOIN (SELECT Trainer.id AS id, COUNT(*) AS cnt
FROM Trainer
JOIN CatchedPokemon ON CatchedPokemon.owner_id = Trainer.id
JOIN Pokemon ON CatchedPokemon.pid = Pokemon.id AND Pokemon.type = "Fire" GROUP BY Trainer.id) AS tmp
ON Trainer.id = tmp.id
WHERE tmp.cnt = (SELECT MAX(cnt) FROM (SELECT COUNT(*) AS cnt
FROM Trainer
JOIN CatchedPokemon ON CatchedPokemon.owner_id = Trainer.id
JOIN Pokemon ON CatchedPokemon.pid = Pokemon.id AND Pokemon.type = "Fire" GROUP BY Trainer.id) AS tmp);

# 14
SELECT DISTINCT type FROM Pokemon WHERE id < 10 ORDER BY id DESC;

# 15
SELECT COUNT(*) FROM Pokemon WHERE type != "Fire";

# 16
SELECT name FROM Pokemon WHERE id IN (SELECT before_id FROM Evolution WHERE Evolution.before_id > Evolution.after_id);

# 17
SELECT AVG(CatchedPokemon.level) FROM CatchedPokemon JOIN Pokemon ON CatchedPokemon.pid = Pokemon.id AND Pokemon.type = 'Water';

# 18
SELECT CatchedPokemon.nickname FROM CatchedPokemon
JOIN Gym ON  Gym.leader_id = CatchedPokemon.owner_id
WHERE CatchedPokemon.level = (SELECT MAX(CatchedPokemon.level) FROM CatchedPokemon JOIN Gym ON Gym.leader_id = CatchedPokemon.owner_id);

# 19
SELECT Trainer.name FROM Trainer
WHERE Trainer.hometown = "Blue City" AND
(SELECT AVG(CatchedPokemon.level) FROM CatchedPokemon WHERE CatchedPokemon.owner_id = Trainer.id)
= (SELECT AVG(CatchedPokemon.level) as AVERAGE
FROM CatchedPokemon
JOIN Trainer ON Trainer.id = CatchedPokemon.owner_id AND Trainer.hometown = "Blue City"
GROUP BY Trainer.id
ORDER BY AVERAGE DESC LIMIT 1)
ORDER BY Trainer.name;

# 20
SELECT Pokemon.name FROM CatchedPokemon
JOIN Pokemon ON CatchedPokemon.pid = Pokemon.id AND Pokemon.type = "Electric"
JOIN Evolution ON Evolution.before_id = CatchedPokemon.pid
WHERE CatchedPokemon.owner_id IN (SELECT id FROM Trainer GROUP BY hometown HAVING count(hometown) = 1);

# 21
SELECT Trainer.name, SUM(CatchedPokemon.level) AS LVSUM FROM Gym
JOIN Trainer ON Trainer.id = Gym.leader_id
JOIN CatchedPokemon ON CatchedPokemon.owner_id = Gym.leader_id
GROUP BY Gym.leader_id
ORDER BY LVSUM DESC;

# 22
SELECT City.name FROM City
WHERE (SELECT COUNT(*) FROM Trainer WHERE Trainer.hometown = City.name)
= (SELECT count(*) AS cnt FROM Trainer GROUP BY Trainer.hometown ORDER BY cnt DESC LIMIT 1);

# 23
SELECT Pokemon.name FROM (SELECT DISTINCT CatchedPokemon.pid AS id FROM CatchedPokemon
JOIN Trainer ON CatchedPokemon.owner_id = Trainer.id AND Trainer.hometown = 'Sangnok City') AS tmp1
JOIN (SELECT DISTINCT CatchedPokemon.pid AS id FROM CatchedPokemon
JOIN Trainer ON CatchedPokemon.owner_id = Trainer.id AND Trainer.hometown = 'Brown City') AS tmp2 ON tmp1.id = tmp2.id
JOIN Pokemon ON tmp1.id = Pokemon.id;

# 24
SELECT Trainer.name FROM CatchedPokemon
JOIN Pokemon ON CatchedPokemon.pid = Pokemon.id AND Pokemon.name LIKE "P%"
JOIN Trainer ON CatchedPokemon.owner_id = Trainer.id AND Trainer.hometown = "Sangnok City"
ORDER BY Trainer.name;

# 25
SELECT Trainer.name, Pokemon.name FROM Trainer
JOIN CatchedPokemon ON CatchedPokemon.owner_id = Trainer.id
JOIN Pokemon ON Pokemon.id = CatchedPokemon.pid
ORDER BY Trainer.name, Pokemon.name;

# 26
SELECT Pokemon.name FROM Evolution AS EV1
JOIN Pokemon ON EV1.before_id = Pokemon.id
WHERE NOT EXISTS (SELECT * FROM Evolution AS EV2 WHERE EV2.before_id = EV1.after_id) AND NOT EXISTS (SELECT * FROM Evolution AS EV3 WHERE EV3.after_id = EV1.before_id)
ORDER BY Pokemon.name;

# 27
SELECT CatchedPokemon.nickname FROM CatchedPokemon
JOIN Gym ON CatchedPokemon.owner_id = Gym.leader_id AND Gym.city = 'Sangnok City'
JOIN Pokemon ON CatchedPokemon.pid = Pokemon.id AND Pokemon.type = 'Water'
ORDER BY CatchedPokemon.nickname;

# 28
SELECT Trainer.name FROM Trainer
WHERE (SELECT COUNT(*) FROM CatchedPokemon JOIN Evolution ON Evolution.after_id = CatchedPokemon.pid WHERE CatchedPokemon.owner_id = Trainer.id) >= 3
ORDER BY Trainer.name;

# 29 
SELECT Pokemon.name FROM Pokemon WHERE NOT EXISTS (SELECT * FROM CatchedPokemon WHERE CatchedPokemon.pid = Pokemon.id) ORDER BY Pokemon.name;

# 30
SELECT MAX(CatchedPokemon.level) as MAXLV FROM CatchedPokemon 
JOIN Trainer ON CatchedPokemon.owner_id = Trainer.id
GROUP BY Trainer.hometown
ORDER BY MAXLV DESC;

# 31
SELECT EV1.before_id, PK1.name, PK2.name, PK3.name FROM Evolution AS EV1
LEFT JOIN Pokemon AS PK1 ON PK1.id = EV1.before_id
LEFT JOIN Pokemon AS PK2 ON PK2.id = EV1.after_id
LEFT JOIN Pokemon AS PK3 ON PK3.id = (SELECT after_id FROM Evolution WHERE before_id = EV1.after_id)
WHERE EXISTS (SELECT * FROM Evolution AS EV2 WHERE EV1.after_id = EV2.before_id
AND NOT EXISTS (SELECT * FROM Evolution AS EV3 WHERE EV2.after_id = EV3.before_id));