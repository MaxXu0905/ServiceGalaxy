buildsql -h ./include -H ./src -f auto_select -c select_class -s 200 << EOF
select col1{char[10]}, col2{int} from test_table2 where col3 = :col3{int} and col4 = :col4{datetime};
EOF

buildsql -h ./include -H ./src -f auto_insert -c insert_class -s 200 << EOF
insert into test_table(col1, col2, col3, col4) values(:col1{char[10]}, :col2{int}, :col3{int}, :col4{datetime});
EOF

buildsql -h ./include -H ./src -f auto_update -c update_class -s 200 << EOF
update test_table set col2 = :col2{int}, col3 = :col3{int} where col1 = :col1{char[10]} and col4 = :col4{datetime};
EOF

buildsql -h ./include -H ./src -f auto_delete -c delete_class -s 200 << EOF
delete from test_table where col1 = :col1{char[10]} and col2 = :col2{int} and col3 = :col3{int} and col4 = :col4{datetime};
EOF

buildsql -h ./include -H ./src -f auto_mixed -c mixed_class -s 200 << EOF
select col1{char[10]}, col2{int} from test_table2 where col3 = :col3{int} and col4 = :col4{datetime};
insert into test_table(col1, col2, col3, col4) values(:col1{char[10]}, :col2{int}, :col3{int}, :col4{datetime});
update test_table set col2 = :col2{int}, col3 = :col3{int} where col1 = :col1{char[10]} and col4 = :col4{datetime};
delete from test_table where col1 = :col1{char[10]} and col2 = :col2{int} and col3 = :col3{int} and col4 = :col4{datetime};
EOF

