#pragma once

// Bounded notification action registry shared by the macOS
// UNUserNotificationCenter delegate and its test. macOS delivers an
// action response as a notification identifier string long after the
// click; this map pairs each outstanding request's identifier with the
// click handler, least-recently-used eviction once full.

#include <QString>

#include <functional>
#include <list>
#include <mutex>
#include <unordered_map>

namespace Lane
{

class NotificationActionMap
{
public:
    static constexpr int kMaxEntries = 32;

    // Registers an action for a notification identifier; returns the key
    // to embed as the UNNotificationRequest identifier. Over capacity,
    // the oldest entry is evicted (its action is dropped: the notification
    // still shows, the button is just inert).
    QString add(std::function<void()> action)
    {
        if (!action) {
            return {};
        }
        std::lock_guard<std::mutex> lock(m_mutex);
        const QString key = QStringLiteral("act-%1").arg(m_next++);
        insert(key, std::move(action));
        return key;
    }

    // Runs and removes the action for `key`, if still registered.
    // Returns false when the identifier was never seen or was evicted:
    // callers log rather than faking success.
    bool take(const QString &key)
    {
        std::function<void()> action;
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            auto it = m_map.find(key);
            if (it == m_map.end()) {
                return false;
            }
            action = it->second;
            m_map.erase(it);
            m_lru.remove(key);
        }
        action();
        return true;
    }

    int size() const
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        return static_cast<int>(m_map.size());
    }

    int evictedCount() const
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_evicted;
    }

    bool contains(const QString &key) const
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_map.count(key) != 0;
    }



private:
    // Keys come from add()'s monotonic counter, so a key is never inserted
    // twice and the oldest entry is the one to evict.
    void insert(const QString &key, std::function<void()> action)
    {
        if (m_map.size() >= static_cast<size_t>(kMaxEntries)) {
            const QString oldest = m_lru.back();
            m_lru.pop_back();
            m_map.erase(oldest);
            ++m_evicted;
        }
        m_lru.push_front(key);
        m_map.emplace(key, std::move(action));
    }

    // The macOS notification delegate and UNUserNotificationCenter
    // completion handlers run on Apple-managed queues; every mutation and
    // read goes through this lock so the two sides never race.
    mutable std::mutex m_mutex;
    std::unordered_map<QString, std::function<void()>> m_map;
    std::list<QString> m_lru;
    int m_evicted = 0;
    quint64 m_next = 0;
};

} // namespace Lane
