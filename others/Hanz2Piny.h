// last modified

#ifndef HANZ2PINY_H
#define HANZ2PINY_H

#include <string>
#include <vector>

class Hanz2Piny
{
public:
    using Unicode = uint16_t;
    enum Polyphone
    {
        all,
        name,
        noname
    };

public:
    Hanz2Piny();

    bool isHanziUnicode(const Unicode unicode) const;
    std::vector<std::string> toPinyinFromUnicode(const Unicode hanzi_unicode, const bool with_tone = true) const;

    bool isUtf8(const std::string& s) const;
    int guess_utf8_char_length(const std::string& s, const int pos) const;
    Unicode utf8_to_unicode(const std::string& s, const int pos) const;

    std::vector<std::pair<bool, std::vector<std::string>>> toPinyinFromUtf8(const std::string& s, const bool with_tone = true, const bool replace_unknown = false, const std::string& replace_unknown_with = "") const;

private:
    static const Unicode begin_hanzi_unicode_, end_hanzi_unicode_;
    static const char* pinyin_list_with_tone_[];

public:
    static std::string hanz2pinyin(const std::string& in)
    {
        Hanz2Piny h2p;
        auto r = h2p.toPinyinFromUtf8(in, false);
        std::string out;
        for (auto& s : r)
        {
            for (auto& s1 : s.second)
            {
                out += s1;
            }
        }
        return out;
    }
};

#endif
