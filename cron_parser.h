#include <string>
#include <chrono>
#include <vector>
#include <functional>
#include <stdexcept>
#include <unordered_set>
#include <unordered_map>
#include <regex>
#include <algorithm>
#include <sstream>


namespace scp {

struct Constraint {
    std::int32_t min;
    std::int32_t max;
    std::vector<std::string> chars;
};

Constraint constraints[] = {
    {0, 59, {}},
    {0, 59, {}},
    {0, 23, {}},
    {1, 31, {"l"}},
    {1, 12, {}},
    {0, 7, {"l"}}
};

struct Field {
    std::unordered_set<std::uint8_t> vec;
    std::unordered_set<std::string> chars;
};

enum class FieldType {
    Second,
    Minute,
    Hour,
    DayOfMonth,
    Month,
    DayOfWeek
};

std::unordered_map<FieldType, std::unordered_map<std::string, std::string>> cron_expression_aliases = {
            {FieldType::Month, {
                {"jan", "1"},
                {"feb", "2"},
                {"mar", "3"},
                {"apr", "4"},
                {"may", "5"},
                {"jun", "6"},
                {"jul", "7"},
                {"aug", "8"},
                {"sep", "9"},
                {"oct", "10"},
                {"nov", "11"},
                {"dec", "12"},}
            },
            {FieldType::DayOfWeek,{
                {"sun" , "0"},
                {"mon" , "1"},
                {"tue" , "2"},
                {"wed" , "3"},
                {"thu" , "4"},
                {"fri" , "5"},
                {"sat" , "6"},}
            }
        };


std::vector<std::string> string_split(const std::string& val, const std::string& separator, const std::size_t limit = 0) {
    std::vector<std::string> vec;

    std::size_t pos = 0;
    for(;;) {
        if(limit != 0 && vec.size() == limit) {
            break;
        }

        auto index = val.find(separator, pos);
        if(index == std::string::npos) {
            vec.emplace_back(val.substr(pos));
            break;
        }
        vec.emplace_back(val.substr(pos, index - pos));
        pos = index + separator.size();
    }

    return vec;
}

template<class InputIt, class OutputIt, class UnaryOperation>
OutputIt my_transform(InputIt first1, InputIt last1,
                   OutputIt d_first, UnaryOperation unary_op)
{
    while (first1 != last1)
        *d_first++ = unary_op(*first1++);
 
    return d_first;
}

template<typename Fn>
std::string regex_replace(const std::string& s, const std::regex& regex, Fn fn) {
    std::string val = s;
    auto word_begin = std::sregex_iterator(s.begin(), s.end(), regex);
    auto word_end = std::sregex_iterator();
    std::size_t offset = 0;
    for(auto i = word_begin; i != word_end; ++i) {
        std::string ss = fn(i->str());
        val.replace(i->position() + offset, i->length(), ss);
        offset += ss.length() - i->length();
    }
    return val;
}

class CronParser {
public:
    CronParser(const std::string& cron, const std::time_t& current_time, const std::time_t& start_time, const std::time_t& end_time) {
        _tm = my_localtime(current_time);
        _start_time = start_time;
        _end_time = end_time;

        parse_cron(cron.empty() ? "?": cron);
    }

    std::time_t prev() {
        auto t = find_schedule(false);
        _tm = my_localtime(t);
        return t;
    }

    std::time_t next() {
        auto t = find_schedule(true);
        _tm = my_localtime(t);
        return t;
    }

    bool has_prev() {
        try {
            find_schedule(false);
            return true;
        } catch(...) {
            return false;
        }
    }

    bool has_next() {
        try {
            find_schedule(true);
            return true;
        } catch(...) {
            return false;
        }
    }

private:
    std::tm my_localtime(const std::time_t& t) {
        std::tm tm = {};
    #ifdef __linux__
        localtime_r(&t, &tm);
    #elif _WIN32
        localtime_s(&tm, &t);
    #else
        tm = *localtime(&t);
    #endif

        return tm;
    }

    std::uint8_t last_day_month(const std::tm& tm) const {
        auto t = tm;
        t.tm_mon += 1;
        t.tm_mday = 0;
        std::mktime(&t);
        return static_cast<std::uint8_t>(t.tm_mday);
    }

    bool match_day_month(const std::tm& tm) const {
        auto field_index = static_cast<std::uint8_t>(FieldType::DayOfMonth);
        auto found = std::find(_fields[field_index].vec.cbegin(), _fields[field_index].vec.cend(), tm.tm_mday);
        if(found != _fields[field_index].vec.cend()) {
            return true;
        }
        for(auto c:_fields[field_index].chars) {
            if(c == "l" && last_day_month(tm) == tm.tm_mday) {
                return true;
            }
        }
        return false;
    }

    bool match_day_week(const std::tm& tm) const {
        auto field_index = static_cast<std::uint8_t>(FieldType::DayOfWeek);
        auto found = std::find(_fields[field_index].vec.cbegin(), _fields[field_index].vec.cend(), tm.tm_wday);
        if(found != _fields[field_index].vec.cend()) {
            return true;
        }
        for(auto c:_fields[field_index].chars) {
            if(c == "l" && 0 == tm.tm_wday) {
                return true;
            }
        }
        return false;
    }

    std::time_t find_schedule(bool is_next) const {
        auto const step = is_next ? 1 : -1;
        std::uint32_t step_count = 0;
        std::tm t = _tm;
        auto current_t = mktime(&t);
        while(step_count < MAX_LOOP) {
            step_count++;
            
            if(!is_next) {
                if(_start_time != std::time_t() && _start_time > std::mktime(&t)) {
                    throw std::runtime_error("Out of the timespan range");
                }
            } else {
                if(_end_time != std::time_t() && _end_time < std::mktime(&t)) {
                    throw std::runtime_error("Out of the timespan range");
                }
            }

            // second
            auto field_index = static_cast<std::uint8_t>(FieldType::Second);
            auto found = std::find(_fields[field_index].vec.cbegin(), _fields[field_index].vec.cend(), t.tm_sec);
            if(found == _fields[field_index].vec.cend()) {
                t.tm_sec += step;
                std::mktime(&t);
                continue;
            }

            // min
            field_index = static_cast<std::uint8_t>(FieldType::Minute);
            found = std::find(_fields[field_index].vec.cbegin(), _fields[field_index].vec.cend(), t.tm_min);
            if(found == _fields[field_index].vec.cend()) {
                t.tm_min += step;
                std::mktime(&t);
                continue;
            }

            // hour
            field_index = static_cast<std::uint8_t>(FieldType::Hour);
            found = std::find(_fields[field_index].vec.cbegin(), _fields[field_index].vec.cend(), t.tm_hour);
            if(found == _fields[field_index].vec.cend()) {
                t.tm_hour += step;
                std::mktime(&t);
                continue;
            }

            // day
            if(!match_day_month(t) && !match_day_week(t)) {
                t.tm_mday += step;
                std::mktime(&t);
                continue;
            }

            // month
            field_index = static_cast<std::uint8_t>(FieldType::Month);
            found = std::find(_fields[field_index].vec.cbegin(), _fields[field_index].vec.cend(), t.tm_mon + 1);
            if(found == _fields[field_index].vec.cend()) {
                t.tm_mon += step;
                std::mktime(&t);
                continue;
            }

            auto time = std::mktime(&t);
            if(time == current_t) {
                t.tm_sec += step;
                std::mktime(&t);
                continue;
            }
            
            return time;
        }

        throw std::runtime_error("Invalid expression, loop limit exceeded");
    }

    void parse_cron(const std::string& cron) {
        auto end = static_cast<std::uint8_t>(FIELDS_SIZE);

        auto fields = string_split(cron, " ");
        if(fields.size() > FIELDS_SIZE) {
            throw std::runtime_error("Invalid cron expression");
        }

        for(auto i = static_cast<std::uint8_t>(fields.size()); i != 0; --i, --end) {
            _fields[end - 1] = std::move(parse_fields(fields[i - 1], static_cast<FieldType>(end - 1), constraints[end - 1]));
        }

        const std::string default_values[] = {"0", "*", "*", "*", "*", "*"};
        // use default value
        for(auto i = static_cast<std::uint8_t>(0); i < end; ++i) {
            _fields[i] = std::move(parse_fields(default_values[i], static_cast<FieldType>(i), constraints[i]));
        }
    }

    Field parse_fields(const std::string& val, const FieldType type, const Constraint constraint) {
        std::unordered_set<std::uint8_t> set;
        std::unordered_set<std::string> chars;

        std::string _val = val;
        my_transform(_val.cbegin(), _val.cend(), _val.begin(), [](const char c) {
            return std::tolower(c);
        });

        if(type == FieldType::Month || type == FieldType::DayOfWeek) {
            std::regex word_regex("/[a-z]{3}/");
            _val = regex_replace(_val, word_regex, [&](const std::string& s) {
                return cron_expression_aliases[type][s];
            });
        }
        
        auto fields = string_split(_val, ",");
        for(auto& field:fields) {
            parse_repeat(field, set, chars, type, constraint);
        }

        return {set, chars};
    }

    void parse_repeat(const std::string& val, std::unordered_set<std::uint8_t>& set, std::unordered_set<std::string>& chars,
                        const FieldType type, const Constraint constraint) {
        std::uint8_t step = 1;
        auto found = val.find("/");
        if(found != std::string::npos && found < val.size() - 1) {
            step = static_cast<std::uint8_t>(std::stoul(val.substr(found + 1)));
        }
        std::string field = val.substr(0, found);
        if(field == "*") {
            field = std::to_string(constraint.min) + "-" + std::to_string(constraint.max);
        }
        
        parse_range(field, step, set, chars, type, constraint);
    }

    void parse_range(const std::string& val, const std::uint8_t step, std::unordered_set<std::uint8_t>& set, std::unordered_set<std::string>& chars,
                        const FieldType type, const Constraint constraint) {

        if(std::find(constraint.chars.cbegin(), constraint.chars.cend(), val) != constraint.chars.cend()) {
            chars.emplace(val);
            return;
        } else if(val == "?" && (type == FieldType::DayOfMonth || type == FieldType::DayOfWeek)) {
            return;
        }


        std::uint8_t min = constraint.min, max = constraint.max;

        auto found = val.find("-");
        if(found == val.npos) {
             set.emplace(static_cast<std::uint8_t>(std::stoul(val)));
             return;
        } else {
            min = static_cast<std::uint8_t>(std::stoul(val.substr(0, found)));
            max = static_cast<std::uint8_t>(std::stoul(val.substr(found + 1)));
        }

        for(auto i = min; i <= max; i+= step) {
            if(type == FieldType::DayOfWeek) {
                set.emplace(i % constraint.max);
            } else {
                set.emplace(i);
            }
        }
    }

private:
    std::tm _tm;
    std::time_t _start_time, _end_time;
    
    static constexpr std::uint8_t const& FIELDS_SIZE = sizeof(constraints)/sizeof(constraints[0]);
    Field _fields[FIELDS_SIZE];
    static constexpr std::uint32_t MAX_LOOP = 10000;

};

} // namespace scp