buildsql -d DBC -h ./include -H ./src -f dbc_test_select -c dbc_test_select_class -s 200 << EOF
select id{char[18]}, test_int_t{int} from dbc_test_apicall where id = :id{char[18]};
EOF

buildsql -d DBC -h ./include -H ./src -f dbc_test_insert -c dbc_test_insert_class -s 200 << EOF
insert into dbc_test_apicall(id, test_int_t) values(:id{char[18]}, :test_int_t{int});
EOF

buildsql -d DBC -h ./include -H ./src -f dbc_test_update -c dbc_test_update_class -s 200 << EOF
update dbc_test_apicall set test_int_t=:test_int_t{int} where id = :id{char[18]};
EOF

buildsql -d DBC -h ./include -H ./src -f dbc_test_delete -c dbc_test_delete_class -s 200 << EOF
delete from dbc_test_apicall where id = :id{char[18]};
EOF

buildsql -d DBC -h ./include -H ./src -f dbc_test_mselect -c dbc_test_mselect_class -s 200 << EOF
select id{char[18]}, test_int_t{int} from dbc_test_apicall where test_int_t = :test_int_t{int};
EOF

buildsql -d DBC -h ./include -H ./src -f dbc_test_mupdate -c dbc_test_mupdate_class -s 200 << EOF
update dbc_test_apicall set test_int_t=:test_int_t1{int} where test_int_t = :test_int_t{int};
EOF

buildsql -d DBC -h ./include -H ./src -f dbc_test_mdelete -c dbc_test_mdelete_class -s 200 << EOF
delete from dbc_test_apicall where test_int_t = :test_int_t{int};
EOF