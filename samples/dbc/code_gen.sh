buildsql -d DBC -h ./include -H ./src -f auto_select -c select_class -s 200 << EOF
select default_region_code{char}, exp_date{datetime} from bds_area_code where area_code = :area_code{char[8]} and eff_date = :eff_date{datetime};
EOF

buildsql -d DBC -h ./include -H ./src -f auto_insert -c insert_class -s 200 << EOF
insert into bds_area_code(default_region_code, area_code, eff_date, exp_date) values(:default_region_code{char}, :area_code{char[8]}, :eff_date{datetime}, :exp_date{datetime});
EOF

buildsql -d DBC -h ./include -H ./src -f auto_update -c update_class -s 200 << EOF
update bds_area_code set default_region_code = :default_region_code{char} where area_code = :area_code{char[8]} and eff_date = :eff_date{datetime};
EOF

buildsql -d DBC -h ./include -H ./src -f auto_delete -c delete_class -s 200 << EOF
delete from bds_area_code where area_code = :area_code{char[8]};
EOF

