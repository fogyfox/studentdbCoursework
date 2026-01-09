-- name: get_user_by_login
SELECT id, login, password_hash, role, first_name, last_name FROM users WHERE login = $1

-- name: insert_user
INSERT INTO users (login, password_hash, role, first_name, last_name) VALUES ($1, $2, $3, $4, $5) RETURNING id

-- name: get_all_users
SELECT id, login, role, first_name, last_name FROM users ORDER BY id

-- name: update_user
UPDATE users SET login=$1, role=$2, first_name=$3, last_name=$4 WHERE id=$5

-- name: update_user_password
UPDATE users SET password_hash=$1 WHERE id=$2

-- name: delete_user
DELETE FROM users WHERE id=$1

-- name: get_user_id_by_student
SELECT user_id FROM students WHERE id = $1

-- name: delete_user_by_id
DELETE FROM users WHERE id = $1

-- name: get_user_id_by_teacher
SELECT user_id FROM teachers WHERE id = $1

-- name: get_auth_data
SELECT id, password, role FROM users WHERE login = $1

-- name: insert_student
INSERT INTO students (user_id, dob, group_id) VALUES ($1, $2, $3)

-- name: get_all_students
SELECT s.id, s.user_id, u.first_name, u.last_name, u.login, s.dob, s.group_id FROM students s LEFT JOIN users u ON s.user_id = u.id ORDER BY s.id


-- name: get_students_by_group
SELECT s.id, s.user_id, u.first_name, u.last_name, u.login, s.dob, s.group_id FROM students s JOIN users u ON s.user_id = u.id WHERE s.group_id = $1 ORDER BY u.last_name

-- name: update_student
UPDATE students SET dob=$1, group_id=$2 WHERE id=$3

-- name: delete_student
DELETE FROM students WHERE id = $1

-- name: get_student_grades
SELECT c.id as course_id, c.name as course_name, g.grade, l.lesson_date as date_assigned 
FROM grades g 
JOIN lessons l ON g.lesson_id = l.id 
JOIN courses c ON l.course_id = c.id 
WHERE g.student_id = $1 
ORDER BY l.lesson_date DESC

-- name: get_student_profile
SELECT s.id, u.first_name, u.last_name, u.login, s.dob, g.name as group_name 
FROM students s 
JOIN users u ON s.user_id = u.id 
LEFT JOIN groups g ON s.group_id = g.id 
WHERE s.id = $1

-- name: get_group_rating
SELECT u.first_name, u.last_name, AVG(CASE WHEN g.grade ~ '^[0-9]+$' THEN g.grade::integer ELSE NULL END) as avg_grade FROM students s JOIN users u ON s.user_id = u.id LEFT JOIN grades g ON s.id = g.student_id WHERE s.group_id = (SELECT group_id FROM students WHERE id = $1) GROUP BY s.id, u.first_name, u.last_name ORDER BY avg_grade DESC NULLS LAST

-- name: get_sid_by_uid
SELECT id FROM students WHERE user_id = $1

-- name: insert_group
INSERT INTO groups (name) VALUES ($1)

-- name: get_all_groups
SELECT g.id, g.name, COUNT(s.id) as student_count 
FROM groups g 
LEFT JOIN students s ON g.id = s.group_id 
GROUP BY g.id, g.name 
ORDER BY g.id

-- name: get_group_by_id
SELECT id, name FROM groups WHERE id = $1

-- name: delete_group
DELETE FROM groups WHERE id = $1

-- name: update_group
UPDATE groups SET name = $1 WHERE id = $2

-- name: get_group_members
SELECT u.first_name, u.last_name, 
       AVG(CASE WHEN g.grade ~ '^[0-9]+$' THEN g.grade::integer ELSE NULL END) as avg_grade 
FROM students s 
JOIN users u ON s.user_id = u.id 
LEFT JOIN grades g ON s.id = g.student_id 
WHERE s.group_id = (SELECT group_id FROM students WHERE id = $1) 
GROUP BY s.id, u.first_name, u.last_name 
ORDER BY avg_grade DESC NULLS LAST

-- name: get_students_by_group_
SELECT s.id, u.first_name, u.last_name FROM students s JOIN users u ON s.user_id = u.id WHERE s.group_id = $1 ORDER BY u.last_name, u.first_name

-- name: insert_course
INSERT INTO courses (name) VALUES ($1)

-- name: get_all_courses
SELECT id, name FROM courses ORDER BY id

-- name: update_course
UPDATE courses SET name=$1 WHERE id=$2

-- name: delete_course
DELETE FROM courses WHERE id=$1

-- name: get_lessons
SELECT id, lesson_date FROM lessons WHERE course_id=$1 AND group_id=$2 ORDER BY lesson_date

-- name: get_lesson_by_course_date
SELECT id FROM lessons WHERE course_id=$1 AND lesson_date=$2

-- name: get_lesson_by_id
SELECT id FROM lessons WHERE course_id = $1 AND lesson_date = $2

-- name: get_lesson_id
SELECT id FROM lessons WHERE course_id = $1 AND lesson_date = $2

-- name: create_lesson
INSERT INTO lessons (course_id, group_id, lesson_date, homework) VALUES ($1, $2, $3, $4) RETURNING id

-- name: get_journal_lessons
SELECT id, lesson_date, homework FROM lessons WHERE course_id = $1 AND group_id = $2 ORDER BY lesson_date

-- name: get_grade_by_student_lesson
SELECT grade FROM grades WHERE student_id=$1 AND lesson_id=$2

-- name: upsert_grade
INSERT INTO grades (student_id, lesson_id, grade) VALUES ($1, $2, $3) ON CONFLICT (student_id, lesson_id) DO UPDATE SET grade = EXCLUDED.grade

-- name: get_all_teachers
SELECT u.id AS user_id, u.login, u.first_name, u.last_name, g.id AS group_id, g.name AS group_name FROM users u LEFT JOIN teacher_groups tg ON u.id = tg.teacher_id LEFT JOIN groups g ON tg.group_id = g.id WHERE u.role='TEACHER' ORDER BY u.last_name

-- name: get_teacher_base_info
SELECT t.id, u.first_name, u.last_name, u.login FROM teachers t JOIN users u ON t.user_id = u.id WHERE u.id = $1

-- name: insert_teacher
INSERT INTO teachers (user_id) VALUES ($1) RETURNING id

-- name: insert_teacher_group
INSERT INTO teacher_groups (teacher_id, group_id) VALUES ($1, $2)

-- name: delete_teacher_groups
DELETE FROM teacher_groups WHERE teacher_id=$1

-- name: get_teacher_by_user_id
SELECT id, first_name, last_name FROM users WHERE id = $1 AND role = 'TEACHER'

-- name: get_teacher_groups_list
SELECT g.id, g.name FROM teacher_groups tg JOIN groups g ON tg.group_id = g.id WHERE tg.teacher_id = $1

-- name: insert_user_teacher
INSERT INTO users (login, password_hash, role, first_name, last_name) VALUES ($1, $2, $3, $4, $5) RETURNING id

-- name: insert_teacher_groups
INSERT INTO teacher_groups (teacher_id, group_id) VALUES ($1, $2)

-- name: sync_teachers
INSERT INTO teachers (user_id) SELECT id FROM users WHERE role = 'TEACHER' ON CONFLICT DO NOTHING

-- name: get_admin_teachers
-- name: get_admin_teachers
SELECT t.id, u.first_name, u.last_name, u.login, STRING_AGG(g.name, ', ') as group_names FROM teachers t JOIN users u ON t.user_id = u.id LEFT JOIN teacher_groups tg ON u.id = tg.teacher_id LEFT JOIN groups g ON tg.group_id = g.id GROUP BY t.id, u.first_name, u.last_name, u.login ORDER BY u.last_name


-- name: insert_teacher_profile
INSERT INTO teachers (user_id) VALUES ($1) RETURNING id

-- name: add_teacher_load
INSERT INTO teacher_courses (teacher_id, course_id, group_id) VALUES ($1, $2, $3)

-- name: get_all_teacher_loads
SELECT tc.teacher_id, tc.course_id, tc.group_id, u.first_name, u.last_name, c.name as course_name, g.name as group_name FROM teacher_courses tc JOIN teachers t ON tc.teacher_id = t.id JOIN users u ON t.user_id = u.id JOIN courses c ON tc.course_id = c.id JOIN groups g ON tc.group_id = g.id ORDER BY u.last_name, c.name

-- name: delete_teacher_load
DELETE FROM teacher_courses WHERE teacher_id = $1 AND course_id = $2 AND group_id = $3

-- name: link_teacher_group
INSERT INTO teacher_groups (teacher_id, group_id) VALUES ($1, $2)

-- name: get_teacher_courses
SELECT c.id, c.name FROM teacher_courses tc JOIN courses c ON c.id = tc.course_id WHERE tc.teacher_id = $1

-- name: get_groups_by_course
SELECT DISTINCT g.id, g.name FROM lessons l JOIN groups g ON g.id = l.group_id WHERE l.course_id = $1

-- name: get_journal_grades
SELECT student_id, lesson_id, grade FROM grades WHERE lesson_id IN (SELECT id FROM lessons WHERE course_id = $1 AND group_id = $2)

-- name: update_teacher_user
UPDATE users SET login=$1, first_name=$2, last_name=$3 WHERE id=$4

-- name: get_grades_for_predict
SELECT g.grade 
FROM grades g 
JOIN lessons l ON g.lesson_id = l.id 
WHERE g.student_id = $1 AND l.course_id = $2 
ORDER BY l.lesson_date ASC

-- name: get_student_by_user_id
SELECT s.id, s.user_id, u.first_name, u.last_name, u.login, s.dob, s.group_id FROM students s JOIN users u ON s.user_id = u.id WHERE s.user_id = $1

-- name: get_group_list_avg
SELECT u.first_name, u.last_name, AVG(CAST(g.grade AS INTEGER)) as avg_score FROM students s JOIN users u ON s.user_id = u.id LEFT JOIN grades g ON s.id = g.student_id WHERE s.group_id = $1 GROUP BY u.id, u.first_name, u.last_name ORDER BY avg_score DESC

-- name: get_grades_by_student_course
SELECT l.lesson_date, g.grade FROM grades g JOIN lessons l ON l.id = g.lesson_id WHERE g.student_id = $1 AND l.course_id = $2

