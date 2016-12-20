
#ifndef sqlite_orm_h
#define sqlite_orm_h

#include <string>
#include <tuple>
#include <type_traits>
#include <sqlite3.h>
#include <functional>
#include <iostream>
#include <stdexcept>
#include <vector>
#include <sstream>
#include <algorithm>
#include <memory>
#include <typeinfo>

using std::cout;
using std::endl;

namespace sqlite_orm {
    
    //  got from here http://stackoverflow.com/questions/25958259/how-do-i-find-out-if-a-tuple-contains-a-type
    template <typename T, typename Tuple>
    struct has_type;
    
    template <typename T>
    struct has_type<T, std::tuple<>> : std::false_type {};
    
    template <typename T, typename U, typename... Ts>
    struct has_type<T, std::tuple<U, Ts...>> : has_type<T, std::tuple<Ts...>> {};
    
    template <typename T, typename... Ts>
    struct has_type<T, std::tuple<T, Ts...>> : std::true_type {};
    
    template <typename T, typename Tuple>
    using tuple_contains_type = typename has_type<T, Tuple>::type;
    
    struct autoincrement {};
    
    struct not_null {};
    
    struct primary_key {};
    
    template<class T, T t>
    struct default_value {
        typedef T value_type;
        
        static value_type value = t;
    };
    
    template<class O, class T, class ...Op>
    struct column {
        typedef O object_type;
        typedef T field_type;
        typedef std::tuple<Op...> options_type;
        typedef field_type object_type::*member_pointer_t;
        
        const std::string name;
        field_type object_type::*member_pointer;
        
        /*constexpr bool has_autoincrement() {
         return has<autoincrement>();
         }
         
         constexpr bool has_not_null() {
         return has<not_null>();
         }*/
        
        template<class Opt>
        constexpr bool has() {
            return tuple_contains_type<Opt, options_type>::value;
        }
        
        /*template<class O1, class O2, class ...Opts>
         constexpr bool has_any() {
         if(has<O1>() || has<O2>()) {
         return true;
         }else{
         return this->has_any<Opts...>();
         }
         }
         
         template<class O1>
         constexpr bool has_any() {
         return has<O1>();
         }*/
        
        //        template<class ...Opt>
        //        constexpr bool has_any();
        
        /*template<class Opt, class ...Opts>
         constexpr bool has_any() {
         if(has<Opt>()){
         return true;
         }else{
         return this->has_any<Opts...>();
         }
         }
         
         constexpr bool has_any() {
         return false;
         }*/
        
        template<class O1, class O2, class ...Opts>
        constexpr bool has_every() {
            if(has<O1>() && has<O2>()) {
                return true;
            }else{
                return has_every<Opts...>();
            }
        }
        
        template<class O1>
        constexpr bool has_every() {
            return has<O1>();
        }
    };
    
    /**
     *  Column builder function. You should use it to create columns and not constructor.
     */
    template<class O, class T, class ...Op>
    column<O, T, Op...> make_column(const std::string &name, T O::*m, Op ...options){
        return {name, m};
    }
    
    /**
     *  Common case for table_impl class.
     */
    template<typename... Args>
    struct table_impl {
        
        std::vector<std::string> column_names() { return {}; }
        
        template<class ...Op>
        std::vector<std::string> column_names_exept() { return {}; }
        
        template<class ...Op>
        std::vector<std::string> column_names_with() { return{}; }
        
        template<class L>
        void for_each_column(L l) {}
        
        template<class Op, class L>
        void for_each_column_exept(L l){}
        
        template<class Op, class L>
        void for_each_column_with(L l) {}
        
        /*template<class ...Op, class L>
         void for_each_column_exept(L l){}
         
         template<class ...Op, class L>
         void for_each_column_with(L l) {}*/
    };
    
    template<typename H, typename... T>
    struct table_impl<H, T...> : private table_impl<T...> {
        typedef H column_type;
        typedef std::tuple<T...> tail_types;
        
        table_impl(H h, T ...t) : col(h), Super(t...) {}
        
        column_type col;
        
        /**
         *  column_names implementation. Notice that result will be reversed.
         *  It is reversed back in `table` class.
         */
        std::vector<std::string> column_names() {
            auto res = Super::column_names();
            res.emplace_back(col.name);
            return res;
        }
        
        /**
         *  column_names_with implementation. Notice that result will be reversed.
         *  It is reversed back in `table` class.
         *  @return vector of column names that have specified Op... conditions.
         */
        template<class ...Op>
        std::vector<std::string> column_names_with() {
            auto res = Super::template column_names_with<Op...>();
            if(col.template has_every<Op...>()) {
                res.emplace_back(col.name);
            }
            return res;
        }
        
        /**
         *  For each implementation. Calls templated lambda with its column
         *  and passed call to superclass.
         */
        template<class L>
        void for_each_column(L l){
            l(col);
            Super::for_each_column(l);
        }
        
        /**
         *  Working version of `for_each_column_exept`. Calls lambda and fire super's function.
         */
        template<class Op, class L, std::enable_if<!tuple_contains_type<Op, typename column_type::options_type>::value>>
        void for_each_column_exept(L l) {
            l(col);
            Super::template for_each_column_exept<Op, L>(l);
        }
        
        /**
         *  Version of `for_each_column_exept` for case if column has not option. Doesn't call lambda and just fure super's function.
         */
        template<class Op, class L>
        void for_each_column_exept(L l) {
            Super::template for_each_column_exept<Op, L>(l);
        }
        
        /**
         *  Working version of `for_each_column_with`. Calls lambda and fire super's function.
         */
        template<class Op, class L, std::enable_if<tuple_contains_type<Op, typename column_type::options_type>::value>>
        void for_each_column_with(L l) {
            l(col);
            Super::template for_each_column_with<Op, L>(l);
        }
        
        /**
         *  Version of `for_each_column_with` for case if column has not option. Doesn't call lambda and just fure super's function.
         */
        template<class Op, class L>
        void for_each_column_with(L l) {
            Super::template for_each_column_with<Op, L>(l);
        }
    private:
        typedef table_impl<T...> Super;
    };
    
    /**
     *  Table interface class. Implementation is hidden in `table_impl` class.
     */
    template<class ...Cs>
    struct table {
        typedef table_impl<Cs...> impl_type;
        typedef typename std::tuple_element<0, std::tuple<Cs...>> object_type;
        
        /**
         *  Table name.
         */
        const std::string name;
        
        /**
         *  Implementation that stores columns information.
         */
        impl_type impl;
        
        /**
         *  @return vector of column names of table.
         */
        std::vector<std::string> column_names() {
            auto res = impl.column_names();
            std::reverse(res.begin(),
                         res.end());
            return res;
        }
        
        /**
         *  @return vector of column names that have options provided as template arguments (not_null, autoincrement).
         */
        template<class ...Op>
        std::vector<std::string> column_names_with() {
            auto res = impl.template column_names_with<Op...>();
            std::reverse(res.begin(),
                         res.end());
            return res;
        }
        
        /**
         *  Iterates all columns and fires passed lambda. Lambda must have one and only templated argument Otherwise code will
         *  not compile.
         *  @param L Lambda type. Do not specify it explicitly.
         *  @param l Lambda to be called per column itself. Must have signature like this [] (auto col) -> void {}
         */
        template<class L>
        void for_each_column(L l) {
            impl.for_each_column(l);
        }
        
        /**
         *  Iterates all columns exept ones that have specified options and fires passed lambda. 
         *  Lambda must have one and only templated argument Otherwise code will not compile.
         *  @param L Lambda type. Do not specify it explicitly.
         *  @param l Lambda to be called per column itself. Must have signature like this [] (auto col) -> void {}
         */
        template<class Op, class L>
        void for_each_column_exept(L l) {
            impl.template for_each_column_exept<Op>(l);
        }
        
        /**
         *  Iterates all columns that have specified options and fires passed lambda.
         *  Lambda must have one and only templated argument Otherwise code will not compile.
         *  @param L Lambda type. Do not specify it explicitly.
         *  @param l Lambda to be called per column itself. Must have signature like this [] (auto col) -> void {}
         */
        template<class Op, class L>
        void for_each_column_with(L l) {
            impl.template for_each_column_with<Op>(l);
        }
        
    };
    
    /**
     *  Function used for table creation. Do not use table constructor - use this function
     *  cause table class is templated and its constructing too (just like std::make_shared or std::make_pair).
     */
    template<class ...Cs>
    table<Cs...> make_table(const std::string &name, Cs ...args) {
        return {name, table_impl<Cs...>(args...)};
    }
    
    /**
     *  Helper class used for binding fields to sqlite3 statements.
     */
    template<class V>
    struct statement_binder {
        
        int bind(sqlite3_stmt*, int index, const V &value);
    };
    
    /**
     *  Specialization for int.
     */
    template<>
    int statement_binder<int>::bind(sqlite3_stmt *stmt, int index, const int &value) {
        return sqlite3_bind_int(stmt, index++, value);
    }
    
    /**
     *  Specialization for std::string.
     */
    template<>
    int statement_binder<std::string>::bind(sqlite3_stmt *stmt, int index, const std::string &value) {
        return sqlite3_bind_text(stmt, index++, value.c_str(), -1, SQLITE_TRANSIENT);
    }
    
    /*template<class T>
     int statement_binder<std::shared_ptr<T>>::bind(sqlite3_stmt *stmt, int index, const std::shared_ptr<T> &value) {
     if(value){
     
     }else{
     
     }
     }*/
    
    /**
     *  Helper class used to cast values from argv to V class 
     *  which depends from column type.
     *
     */
    template<class V>
    struct row_extrator {
        V extract(const char *row_value);
    };
    
    /**
     *  Specialization for int.
     */
    template<>
    struct row_extrator<int> {
        int extract(const char *row_value) {
            return std::atoi(row_value);
        }
    };
    
    /**
     *  Specialization for std::string.
     */
    template<>
    struct row_extrator<std::string> {
        std::string extract(const char *row_value) {
            return row_value;
        }
    };
    
    /**
     *  Exeption thrown if nothing was found in database with specified id.
     */
    struct not_found_exception : public std::exception {
        
        virtual const char* what() const throw() override {
            return "Not found";
        };
    };
    
    template<class T>
    struct storage {
        typedef T table_type;
        
        /**
         *  @param filename database filename.
         *  @param table table created by function `make_table`
         */
        storage(const std::string &filename_, T table_):filename(filename_), table(table_){}
        
        /**
         *  Help class. Used to pass data to sqlite C callback. Doesn't need
         *  to be used explicitly by user.
         */
        template<class O>
        struct data_t {
            storage<T>& t;
            O *res;
        };
        
        /**
         *  Delete routine. 
         *  @param O Object's type. Must be specified explicitly.
         *  @param id id of object to be removed.
         */
        template<class O>
        void remove(int id) throw (std::runtime_error) {
            withDatabase([&](auto db) {
                std::stringstream ss;
                ss << "delete from " << table.name;
                ss << " where ";
                std::vector<std::string> primaryKeyColumnNames;
                table.template for_each_column([&] (auto c) {
                    if(c.template has<primary_key>()) {
                        primaryKeyColumnNames.emplace_back(c.name);
                    }
                });
                //                auto primaryKeyColumnNames = table.template column_names_with<primary_key>();
                for(auto i = 0; i < primaryKeyColumnNames.size(); ++i) {
                    ss << primaryKeyColumnNames[i] << " =  ?";// + std::to_string(id);
                    if(i < primaryKeyColumnNames.size() - 1) {
                        ss << " and ";
                    }else{
                        ss << " ";
                    }
                }
                auto query = ss.str();
//                cout<<"query = "<<query<<endl;
                sqlite3_stmt *stmt;
                if (sqlite3_prepare_v2(db, query.c_str(), -1, &stmt, nullptr) == SQLITE_OK) {
                    auto index = 1;
                    table.template for_each_column_with<primary_key>([&] (auto c) {
                        //                        if(c.template has<primary_key>()) {
                        //                            auto &value = o.*c.member_pointer;
                        typedef typename decltype(c)::field_type field_type;
//                        cout<<"field_type = "<<typeid(field_type).name()<<endl;
                        statement_binder<field_type>().bind(stmt, index++, std::to_string(id));
                        //                        cout<<"c.name = "<<c.name<<", has primary_key = "<<c.template has<primary_key>()<<endl;
                        //                        }
                    });
                    if (sqlite3_step(stmt) == SQLITE_DONE) {
                        return;
                    }else{
                        throw std::runtime_error(sqlite3_errmsg(db));
                    }
                }else{
                    throw std::runtime_error(sqlite3_errmsg(db));
                }
            });
        }
        
        /**
         *  Update routine. Sets all non primary key fields where primary key is equal.
         *  @param O Object type. May be not specified explicitly cause it can be deduced by
         *      compiler from first parameter.
         *  @param o object to be updated.
         */
        template<class O>
        void update(const O &o) throw (std::runtime_error) {
            withDatabase([&](auto db) {
                std::stringstream ss;
                ss << "update " << table.name << " set ";
                std::vector<std::string> setColumnNames;
                table.template for_each_column( [&] (auto c) {
                    if(!c.template has<primary_key>()) {
                        setColumnNames.emplace_back(c.name);
                    }
                });
                //                auto setColumnNames = table.template column_names_exept<primary_key>();
                for(auto i = 0; i < setColumnNames.size(); ++i) {
                    ss << setColumnNames[i] << " = ?";
                    if(i < setColumnNames.size() - 1) {
                        ss << ", ";
                    }else{
                        ss << " ";
                    }
                }
                ss << "where ";
                auto primaryKeyColumnNames = table.template column_names_with<primary_key>();
                for(auto i = 0; i < primaryKeyColumnNames.size(); ++i) {
                    ss << primaryKeyColumnNames[i] << " = ?";
                    if(i < primaryKeyColumnNames.size() - 1) {
                        ss << " and ";
                    }else{
                        ss << " ";
                    }
                }
                
                //                auto query = "update " + table.name + " set name = ? where id = ?";
                auto query = ss.str();
                //                cout<<"query = "<<query<<endl;
                sqlite3_stmt *stmt;
                if (sqlite3_prepare_v2(db, query.c_str(), -1, &stmt, nullptr) == SQLITE_OK) {
                    auto index = 1;
                    table.template for_each_column([&] (auto c) {
                        if(!c.template has<primary_key>()) {
                            auto &value = o.*c.member_pointer;
                            statement_binder<typename decltype(c)::field_type>().bind(stmt, index++, value);
                        }
                    });
                    table.template for_each_column([&] (auto c) {
                        if(c.template has<primary_key>()) {
                            auto &value = o.*c.member_pointer;
                            statement_binder<typename decltype(c)::field_type>().bind(stmt, index++, value);
                        }
                    });
                    if (sqlite3_step(stmt) == SQLITE_DONE) {
                        return;
                    }else{
                        throw std::runtime_error(sqlite3_errmsg(db));
                    }
                }else {
                    throw std::runtime_error(sqlite3_errmsg(db));
                }
            });
        }
        
        /**
         *  Select * with no conditions routine.
         *  @param O Object type to be extracted. Must be specified explicitly.
         *  @return All objects of type O stored in database at the moment.
         */
        template<class O>
        std::vector<O> get_all() throw(std::runtime_error) {
            std::vector<O> res;
            withDatabase([&](auto db) {
                auto query = "select * from " + table.name;
                data_t<std::vector<O>> data{*this, &res};
                auto rc = sqlite3_exec(db,
                                       query.c_str(),
                                       [](void *data, int argc, char **argv,char **azColName) -> int {
                                           auto &d = *(data_t<std::vector<O>>*)data;
                                           auto &res = *d.res;
                                           auto t = d.t;
                                           if(argc){
                                               O o;
                                               auto index = 0;
                                               t.table.for_each_column([&] (auto c) {
                                                   auto member_pointer = c.member_pointer;
                                                   auto value = row_extrator<typename decltype(c)::field_type>().extract(argv[index++]);
                                                   o.*member_pointer = value;
                                               });
                                               res.emplace_back(std::move(o));
                                           }
                                           return 0;
                                       }, &data, nullptr);
                if(rc != SQLITE_OK) {
                    throw std::runtime_error(sqlite3_errmsg(db));
                }
            });
            return res;
        }
        
        /**
         *  Select * by id routine.
         *  @param O Object type to be extracted. Must be specified explicitly.
         *  @return Object of type O where id is equal parameter passed or throws `not_found_exception`
         *  if there is no object with such id.
         */
        template<class O>
        O get(int id) throw (not_found_exception, std::runtime_error) {
            std::shared_ptr<O> res;
            withDatabase([&](auto db) {
                auto query = "select * from " + table.name + " where id = " + std::to_string(id);
                data_t<std::shared_ptr<O>> data{*this, &res};
                auto rc = sqlite3_exec(db,
                                       query.c_str(),
                                       [](void *data, int argc, char **argv,char **azColName)->int{
                                           auto &d = *(data_t<std::shared_ptr<O>>*)data;
                                           auto &res = *d.res;
                                           auto t = d.t;
                                           if(argc){
                                               res = std::make_shared<O>();
                                               //                                               O o;
                                               auto index = 0;
                                               t.table.for_each_column([&] (auto c) {
                                                   auto &o = *res;
                                                   auto member_pointer = c.member_pointer;
                                                   auto value = row_extrator<typename decltype(c)::field_type>().extract(argv[index++]);
                                                   o.*member_pointer = value;
                                               });
                                           }
                                           return 0;
                                       }, &data, nullptr);
                if(rc != SQLITE_OK) {
                    throw std::runtime_error(sqlite3_errmsg(db));
                }
            });
            if(res){
                return *res;
            }else{
                throw not_found_exception{};
            }
        }
        
        template<class O>
        std::shared_ptr<O> get_no_throw(int id) throw(std::runtime_error) {
            std::shared_ptr<O> res;
            withDatabase([&](auto db) {
                auto query = "select * from " + table.name + " where id = " + std::to_string(id);
                data_t<std::shared_ptr<O>> data{*this, &res};
                auto rc = sqlite3_exec(db,
                                       query.c_str(),
                                       [](void *data, int argc, char **argv,char **azColName)->int{
                                           auto &d = *(data_t<std::shared_ptr<O>>*)data;
                                           auto &res = *d.res;
                                           auto t = d.t;
                                           if(argc){
                                               res = std::make_shared<O>();
                                               auto index = 0;
                                               t.table.for_each_column([&] (auto c) {
                                                   auto &o = *res;
                                                   auto member_pointer = c.member_pointer;
                                                   auto value = row_extrator<typename decltype(c)::field_type>().extract(argv[index++]);
                                                   o.*member_pointer = value;
                                               });
                                           }
                                           return 0;
                                       }, &data, nullptr);
                if(rc != SQLITE_OK) {
                    throw std::runtime_error(sqlite3_errmsg(db));
                }
            });
            return res;
        }
        
        /**
         *  Select count(*) with not conditions routine.
         *  @return Number of O object in table.
         */
        template<class O>
        int count() throw (std::runtime_error) {
            int res = 0;
            withDatabase([&](auto db) {
                auto query = "select count(*) from " + table.name;
                data_t<int> data{*this, &res};
                auto rc = sqlite3_exec(db,
                                       query.c_str(),
                                       [](void *data, int argc, char **argv,char **azColName)->int{
                                           auto &d = *(data_t<int>*)data;
                                           auto &res = *d.res;
                                           auto t = d.t;
                                           if(argc){
                                               res = std::atoi(argv[0]);
                                           }
                                           return 0;
                                       }, &data, nullptr);
                if(rc != SQLITE_OK) {
                    throw std::runtime_error(sqlite3_errmsg(db));
                }
            });
            return res;
        }
        
        /**
         *  Insert routine. Inserts object with all non primary key fields in passed object. Id of passed
         *  object doesn't matter.
         *  @return id of just created object.
         */
        template<class O>
        int insert(const O &o) throw (std::runtime_error) {
            int res = 0;
            withDatabase([&](auto db) {
                std::stringstream ss;
                ss << "INSERT INTO " << table.name << " (";
                std::vector<std::string> columnNames;
                table.template for_each_column([&] (auto c) {
                    if(!c.template has<primary_key>()) {
                        columnNames.emplace_back(c.name);
                    }
                });
                
                for(auto i = 0; i < columnNames.size(); ++i) {
                    ss << columnNames[i];
                    if(i < columnNames.size() - 1) {
                        ss << ", ";
                    }else{
                        ss << ") ";
                    }
                }
                ss << "VALUES(";
                for(auto i = 0; i < columnNames.size(); ++i) {
                    ss << "?";
                    if(i < columnNames.size() - 1) {
                        ss << ", ";
                    }else{
                        ss << ")";
                    }
                }
                auto query = ss.str();
                sqlite3_stmt *stmt;
                if (sqlite3_prepare_v2(db, query.c_str(), -1, &stmt, nullptr) == SQLITE_OK) {
                    auto index = 1;
                    table.template for_each_column([&] (auto c) {
                        if(!c.template has<primary_key>()){
                            auto &value = o.*c.member_pointer;
                            statement_binder<typename decltype(c)::field_type>().bind(stmt, index++, value);
                        }
                    });
                    if (sqlite3_step(stmt) == SQLITE_DONE) {
                        res = int(sqlite3_last_insert_rowid(db));
                    }else{
                        throw std::runtime_error(sqlite3_errmsg(db));
                    }
                }else {
                    throw std::runtime_error(sqlite3_errmsg(db));
                }
            });
            return res;
        }
        
        void checkSchema() {
            
        }
        
    protected:
        std::string filename;
        T table;
        
        void withDatabase(std::function<void(sqlite3*)> lambda) {
            sqlite3 *db;
            auto rc = sqlite3_open(this->filename.c_str(), &db);
            if(rc == SQLITE_OK){
                lambda(db);
            }else{
                std::cerr << "error " << sqlite3_errmsg(db) << std::endl;
            }
            sqlite3_close(db);
        }
    };
    
    template<class T>
    storage<T> make_storage(const std::string &filename, T table) {
        return {filename, table};
    }
    
}

#endif /* sqlite_orm_h */
