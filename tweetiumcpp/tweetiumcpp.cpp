/* Test historical data program for Twitter */

#include <iostream>
#include <map>
#include <iostream>
#include <string>
#include <chrono>
#include <curl/curl.h>
#include <boost/property_tree/json_parser.hpp>
#include <boost/foreach.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>
#include <regex>

#include <chrono>
#include <thread>

//#pragma warning(disable : 4996) //_CRT_SECURE_NO_WARNINGS

using namespace boost::posix_time;
using namespace std;
using boost::property_tree::ptree;

void debug_print(string text) {
    std::cout << text << std::endl;
}

template <typename T = boost::property_tree::ptree>
T element_at(ptree const& pt, std::string name, size_t n) {
    return std::next(pt.get_child(name).find(""), n)->second.get_value<T>();
}

size_t curlwrite_callback(void* contents, size_t size, size_t nmemb, std::string* s)
{
    size_t newLength = size * nmemb;
    try
    {
        s->append((char*)contents, newLength);
    }
    catch (std::bad_alloc& e)
    {
        return 0;
    }
    return newLength;
}


std::string ReplaceAll(std::string str, const std::string& from, const std::string& to) {
    size_t start_pos = 0;
    while ((start_pos = str.find(from, start_pos)) != std::string::npos) {
        str.replace(start_pos, from.length(), to);
        start_pos += to.length();
    }
    return str;
}

std::string create_url_params(std::string base_url, std::vector<std::pair<std::string, std::string>> query_params) {
    int el_count = 0;

    for (auto& element : query_params) {
        if (el_count >= 1) {
            base_url.append("&");
        }
        base_url.append(element.first);
        base_url.append("=");

        element.second = ReplaceAll(element.second, std::string(" "), std::string("%20"));

        base_url.append(element.second);
        el_count++;
    }
    return base_url;
}

std::string extract_cursor(boost::property_tree::ptree timeline) {
    int instruction_count = timeline.get_child("instructions").size();

    if (instruction_count >= 1) {
        if (instruction_count == 1) {
            ptree entries = timeline.get_child("instructions").front().second.get_child("addEntries").get_child("entries");
            ptree cursor = entries.back().second.get_child("content").get_child("operation").get_child("cursor");
            string cursor_value = cursor.get<string>("value");
            return cursor_value;
            
        }
        else {

            ptree entry = timeline.get_child("instructions").back().second.get_child("replaceEntry").get_child("entry");
            ptree cursor = entry.get_child("content").get_child("operation").get_child("cursor");
            string cursor_value = cursor.get<string>("value");
            return cursor_value;
            
        }
    }
    return "NULL";
}


class Tweet {
public:
    int64_t id;
    int64_t retweet_count;
    int64_t favorite_count;
    int64_t user_id;
    string text;
    string date_created_str;
    int created_at;

    Tweet(ptree tweet_data) {
        boost::property_tree::json_parser::write_json("debug.json", tweet_data);
        id = tweet_data.get<int64_t>("id");
        retweet_count = tweet_data.get<int64_t>("retweet_count");
        favorite_count = tweet_data.get<int64_t>("favorite_count");
        user_id = tweet_data.get<int64_t>("user_id");
        text = tweet_data.get<string>("text");
        date_created_str = tweet_data.get<string>("created_at");
        created_at = -1;

    }
};

class TweetResponse {
public:
    vector<Tweet> tweets;
    int get_tweet_count();
    string cursor;
    bool is_last_request;

    Tweet get_tweet_at(int);
    string get_cursor();
    void print_tweets();

    TweetResponse(ptree d) {
        ptree tweets_raw_data = d.get_child("globalObjects.tweets");
        ptree timeline = d.get_child("timeline");
        cursor = extract_cursor(timeline);
        bool hasTweets = false;
        for (auto pair : tweets_raw_data)
        {
            Tweet tweet(pair.second);
            tweets.push_back(tweet);
            hasTweets = true;
        }
        if (!hasTweets) {
            is_last_request = true;
        }
    }
};

int TweetResponse::get_tweet_count() {
    return this->tweets.size();
}

string TweetResponse::get_cursor() {
    return this->cursor;
}

Tweet TweetResponse::get_tweet_at(int pos) {
    return this->tweets.at(pos);
}

void TweetResponse::print_tweets() {
    if (this->get_tweet_count() > 0) {
        for (int i = 0; i < 1/*this->get_tweet_count() */; i++) {
            Tweet t = this->get_tweet_at(i);

            std::string testString(t.text);
            std::regex e("([^\n]|^)\n([^\n]|$)");
            //std::cout << "\033[3;104;30m" << " - [" << t.date_created_str << "] - " << "\033[0m\t\t" <<  std::regex_replace(testString, e, "$1 $2") << std::endl;
            std::cout << "      \033[3;104;30m" << "[+] ... (" << this->get_tweet_count() << ") tweets\033[0m\t\t" << std::endl;
        }
    }
    else {
        std::cout << "      \033[3;104;30m" << "No tweets\033[0m\t\t" << std::endl;
    }
}

TweetResponse make_tweet_request(std::string url) {
    CURL* curl;
    CURLcode res;

    curl_global_init(CURL_GLOBAL_DEFAULT);

    curl = curl_easy_init();
    std::string json_string;
    if (curl)
    {
        curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
        struct curl_slist* chunk = NULL;
        chunk = curl_slist_append(chunk, "Authorization: Bearer AAAAAAAAAAAAAAAAAAAAANRILgAAAAAAnNwIzUejRCOuH5E6I8xnZz4puTs%3D1Zv7ttfk8LF81IUq16cHjhLTvJu4FA33AGWWjCpTnA");
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, chunk);
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 1);
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 1);
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, curlwrite_callback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &json_string);
        res = curl_easy_perform(curl);
        if (res != CURLE_OK)
        {
            fprintf(stderr, "curl_easy_perform() failed: %s\n",
                curl_easy_strerror(res));
        }
        curl_easy_cleanup(curl);

        std::stringstream ss;

        ss << json_string;

        //std::cout << json_string;

        boost::property_tree::ptree pt;
        boost::property_tree::read_json(ss, pt);
        TweetResponse tweetResponse(pt);

        return tweetResponse;


    }
}


TweetResponse prep_tweet_request(std::string search_query, string cursor = "") {

    std::vector<std::pair<std::string, std::string>> query_params;
    //query_params.push_back(std::make_pair("q", search_query.c_str()));
    //query_params.push_back(std::make_pair("src", "typed_query"));
    query_params.push_back(std::make_pair("f", "live"));
    /*
    query_params.push_back(std::make_pair("include_profile_interstitial_type", "1"));
    query_params.push_back(std::make_pair("include_blocking", "1"));
    query_params.push_back(std::make_pair("include_blocked_by", "1"));
    query_params.push_back(std::make_pair("include_followed_by", "1"));
    query_params.push_back(std::make_pair("include_want_retweets", "1"));
    query_params.push_back(std::make_pair("include_mute_edge", "1"));
    query_params.push_back(std::make_pair("include_can_dm", "1"));
    query_params.push_back(std::make_pair("include_can_media_tag", "1"));
    query_params.push_back(std::make_pair("include_ext_has_nft_avatar", "1"));
    query_params.push_back(std::make_pair("skip_status", "1"));
    query_params.push_back(std::make_pair("cards_platform", "Web-12"));
    query_params.push_back(std::make_pair("include_cards", "1"));
    query_params.push_back(std::make_pair("include_ext_alt_text", "true"));
    query_params.push_back(std::make_pair("include_quote_count", "true"));
    query_params.push_back(std::make_pair("include_reply_count", "1"));;
    query_params.push_back(std::make_pair("include_ext_collab_control", "true"));
    query_params.push_back(std::make_pair("include_entities", "true"));
    query_params.push_back(std::make_pair("include_user_entities", "true"));
    query_params.push_back(std::make_pair("include_ext_media_color", "true"));
    query_params.push_back(std::make_pair("include_ext_media_availability", "true"));
    query_params.push_back(std::make_pair("include_ext_sensitive_media_warning", "true"));
    query_params.push_back(std::make_pair("include_ext_trusted_friends_metadata", "true"));**/
    query_params.push_back(std::make_pair("count", "40"));
    query_params.push_back(std::make_pair("send_error_codes", "true"));
    query_params.push_back(std::make_pair("q", search_query));
    query_params.push_back(std::make_pair("query_source", "typed_query"));


    if (cursor != "") {
        query_params.push_back(std::make_pair("cursor", cursor));
    }

    std::string base_url = "https://api.twitter.com/2/search/adaptive.json?";
    std::string url_full = create_url_params(base_url, query_params);
    return make_tweet_request(url_full);
    
}

void get_tweets(string search_query, string since = "", string until = "", double longitude = 0, double latitude = 0){
    //try {

        string cursor = "";
        int total_tweet_count = 0;
        bool done = false;

        while (!done) {
            std::stringstream ss;

            ss << search_query;

            if (since != "" && until != "") {
                ss << " since:" << since << " until:" << until << " ";
            }

            if (longitude != 0 && latitude != 0) {
                ss << " geocode:" << longitude << "," << latitude << ",55.5km ";
            }

            string tweetParams = ss.str();

            //std::cout << tweetParams << endl;
            
            TweetResponse tweetResponse = prep_tweet_request(tweetParams, cursor);

            tweetResponse.print_tweets();

            total_tweet_count += tweetResponse.get_tweet_count();
            cursor = tweetResponse.get_cursor();
            done = tweetResponse.is_last_request;
            
            //std::cout << "\033[3;43;30m" << "Total tweets loaded: " << total_tweet_count << "\033[0m\t\t" << endl;
            //std::cout << "\033[1;47;35m" << "IS_LAST_REQUEST: " << (done == 1 ? "true" : "false") << "\033[0m\t\t" << endl;

            if (done) {
                //std::this_thread::sleep_for(std::chrono::milliseconds(100000000));
            }
        }
/*
    }
    catch (std::exception const& ex)
    {
        cout << ("Can't init settings. %s", ex.what());
    } */
}

int date_to_epoch(std::string date) {
    std::stringstream ss;
    ss << date << " 00:00:00";
    std::string ts(ss.str());
    ptime t(time_from_string(ts));
    ptime start(boost::gregorian::date::date(1970, 1, 1));
    time_duration dur = t - start;
    time_t epoch = dur.total_seconds();
    return epoch;
}

string epoch_to_date(int epochTime) {
    std::stringstream str;
    boost::posix_time::time_facet* facet = new boost::posix_time::time_facet("%Y-%m-%d");
    str.imbue(std::locale(str.getloc(), facet));
    str << boost::posix_time::from_time_t(epochTime);
    return str.str();
}

string get_current_date() {
    std::stringstream str;
    boost::posix_time::time_facet* facet = new boost::posix_time::time_facet("%Y-%m-%d");
    str.imbue(std::locale(str.getloc(), facet));
    str << boost::posix_time::second_clock::universal_time(); //your time point goes here
    return str.str();
}

string to_human_dt(int time) {
    std::time_t btime_ = time;
    std::stringstream dt_text;
    dt_text << boost::posix_time::from_time_t(btime_);
    dt_text.imbue(std::locale(std::cout.getloc(), new boost::posix_time::time_facet("%H:%M:%S")));
    return dt_text.str();
}


// function that saves tweets from a given date to a given date

void do_thread(string search_query, string from_date, string to_date) {

    cout << "\x1B[36m" << "Threading Scanning [" << search_query << "] from [" << from_date << "] to [" << to_date << "]\033[0m\n" << endl;

    int current_scan_epoch = date_to_epoch(from_date);

    bool date_scan_complete = false;

    while (!date_scan_complete) {
        if (current_scan_epoch > date_to_epoch(to_date)) {
            date_scan_complete = true;
            cout << "Date scan complete, returning to main function" << endl;
        }
        else {
            string date_from = epoch_to_date(current_scan_epoch);
            int date_scan_from = current_scan_epoch;
            int date_scan_to = date_scan_from + 86400;

            cout << "\x1B[36m" <<  "> Scanning from " << epoch_to_date(date_scan_from) << " to " << epoch_to_date(date_scan_to) << "]\033[0m\n" << endl;

            bool time_scan_complete = false;

            int current_time_scan = current_scan_epoch;

            while (!time_scan_complete) {
                
                if (current_time_scan >= date_scan_to) {
                    time_scan_complete = true;
                    cout << "\033[3;42;30mCompleted a day\033[0m\t\t" << endl;;
                    std::this_thread::sleep_for(std::chrono::milliseconds(100000));
                }
                else {
                    int epoch_scan_from = current_time_scan;
                    int epoch_scan_to = current_time_scan + 3600;

                    std::cout << "  > Scanning from " << to_human_dt(epoch_scan_from) << " to " << to_human_dt(epoch_scan_to) << endl;


                    get_tweets(search_query, std::to_string(epoch_scan_from), std::to_string(epoch_scan_to));

                    current_time_scan += 60; //3600;
                }
            }

            current_scan_epoch = date_scan_to;
        }
    }

}

int main() {
    std::string search_query = "btc OR BITCOIN OR $btc";
    std::string from_date = "2020-01-01";
    std::string to_date = get_current_date();

    do_thread(search_query, from_date, to_date);
    return 0;
}




/* 
TODO:

- Update create_url_params function to use char list instead of string https://www.geeksforgeeks.org/methods-to-concatenate-string-in-c-c-with-examples/
- LIBCURL Optimizations as saved on bookmarks
- Checkout cursor placement, do we get more results from using scroll:CURSOR

            //ALSO GET TWEET COUNT AS METRIC
            // GET TWEEET METRICS 

            //LOAD USERS DATA



            // CHANGE scan_date_to to use current date
*/



int main2(int argc, char** argv) {

    printf("\n");
    printf("\x1B[31mTexting1\033[0m\t\t");
    printf("\x1B[32mTexting2\033[0m\t\t");
    printf("\x1B[33mTexting3\033[0m\t\t");
    printf("\x1B[34mTexting4\033[0m\t\t");
    printf("\x1B[35mTexting5\033[0m\n");

    printf("\x1B[36mTexting6\033[0m\t\t");
    printf("\x1B[36mTexting7\033[0m\t\t");
    printf("\x1B[36mTexting8\033[0m\t\t");
    printf("\x1B[37mTexting9\033[0m\t\t");
    printf("\x1B[93mTexting10\033[0m\n");

    printf("\033[3;42;30mTexting11\033[0m\t\t");
    printf("\033[3;43;30mTexting12\033[0m\t\t");
    printf("\033[3;44;30mTexting13\033[0m\t\t");
    printf("\033[3;104;30mTexting14\033[0m\t\t");
    printf("\033[3;100;30mTexting15\033[0m\n");

    printf("\033[3;47;35mTexting16\033[0m\t\t");
    printf("\033[2;47;35mTexting17\033[0m\t\t");
    printf("\033[1;47;35mTexting18\033[0m\t\t");
    printf("\t\t");
    printf("\n");

    return 0;
}


/*
Additional TODOs:
- Update searcher to use epoch time in smaller increments
- Update searcher to geolocate tweeters.

*/