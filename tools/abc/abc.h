#pragma once

#include <string>

void initDBFieldInfo();
void trans_bin_list(std::string in, std::string out);
void trans_fight_frame(std::string path0);
void saveR0ToDB(int num, std::string path);
int expandR(std::string idx, std::string grp, int index, std::string path = "./", bool ranger = true, bool make_fightframe = false);
void combine_image_path(std::string in, std::string out);
void combine_ka(std::string in, std::string out);
void check_fight_frame(std::string path, int repair = 0);
void check_script(std::string path);
void make_heads(std::string path);
void split_eft_file(std::string path, std::string eftlist_bin);
