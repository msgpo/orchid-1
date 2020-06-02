/* Orchid - WebRTC P2P VPN Market (on Ethereum)
 * Copyright (C) 2017-2019  The Orchid Authors
*/

/* GNU Affero General Public License, Version 3 {{{ */
/*
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.

 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Affero General Public License for more details.

 * You should have received a copy of the GNU Affero General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
**/
/* }}} */


#ifndef ORCHID_PARALLEL_HPP
#define ORCHID_PARALLEL_HPP

#include <cppcoro/when_all.hpp>

#include "maybe.hpp"

namespace orc {

template <typename ...Args_>
[[nodiscard]] auto Parallel(Task<Args_> &&...args) -> Task<std::tuple<Maybe<Args_>...>> {
#ifdef ORC_FIBER
    const auto parent(co_await co_optic);
    co_return co_await cppcoro::when_all([](Task<Args_> &&task, Fiber *parent) -> cppcoro::task<Maybe<Args_>> {
        Fiber fiber(parent);
        task.Set(&fiber);
        co_return co_await Try(std::move(task));
    }(std::forward<Task<Args_>>(args), parent)...);
#else
    co_return co_await cppcoro::when_all(Try(std::forward<Task<Args_>>(args))...);
#endif
}

template <typename Type_>
[[nodiscard]] auto Parallel(std::vector<Task<Type_>> &&tasks) -> Task<std::vector<Maybe<Type_>>> {
#ifdef ORC_FIBER
    std::vector<Fiber> fibers(tasks.size(), co_await co_optic);
    for (size_t i(0); i != tasks.size(); ++i)
        tasks[i].Set(&fibers[i]);
#endif

    std::vector<Task<Maybe<Type_>>> maybes;
    maybes.reserve(tasks.size());
    for (size_t i(0); i != tasks.size(); ++i)
        maybes.emplace_back(Try(std::move(tasks[i])));

    co_return co_await cppcoro::when_all(std::move(maybes));
}

template <typename Type_, typename Enable_ = std::enable_if_t<!std::is_void_v<decltype(std::declval<Type_>().result())>>>
auto operator *(std::vector<Type_> &&readys) noexcept(noexcept(std::declval<Type_>().result())) {
    std::vector<std::decay_t<decltype(*std::declval<Type_>())>> values;
    for (auto &ready : readys)
        values.emplace_back(std::move(ready).result());
    return values;
}

template <typename Type_, typename Enable_ = std::enable_if_t<std::is_void_v<decltype(std::declval<Type_>().result())>>>
void operator *(std::vector<Type_> &&readys) noexcept(noexcept(std::declval<Type_>().result())) {
    for (auto &ready : readys)
        std::move(ready).result();
}

template <typename ...Args_>
constexpr bool Voided() {
    return (std::is_void_v<decltype(std::declval<Args_>().result())> && ...);
}

template <typename ...Args_, typename Enable_ = std::enable_if_t<!Voided<Args_...>()>>
auto operator *(std::tuple<Args_...> &&readys) {
    return std::apply([](auto && ...ready) {
        return std::make_tuple(std::move(ready).result()...);
    }, std::forward<std::tuple<Args_...>>(readys));
}

template <typename ...Args_, typename Enable_ = std::enable_if_t<Voided<Args_...>()>>
void operator *(std::tuple<Args_...> &&readys) {
    std::apply([](auto && ...ready) {
        (std::move(ready).result(), ...);
    }, std::forward<std::tuple<Args_...>>(readys));
}

}

#endif//ORCHID_PARALLEL_HPP