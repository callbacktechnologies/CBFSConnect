import java.util.HashMap;
import java.util.Map;

/**
 * The storage for global objects (such as contexts) which are returned to a driver
 * and must not be processed by GC until explicitly released.
 */
class globals {
    private static long counter = 0;
    private static final Map<Long, Item> storage = new HashMap<Long, Item>();

    /**
     * Increases the reference counter for a specified global object by its ID.
     * @param id    ID of a stored object to increase the reference counter
     * @return A stored object that corresponds to the specified ID,
     * or {@code null} if no object with the specified ID exists.
     * @see globals#release(long)
     */
    static Object acquire(long id) {
        synchronized (storage) {
            Item item = storage.get(id);
            if (item == null)
                return null;
            item.refs++;
            return item.target;
        }
    }

    /**
     * Adds a new object and initializes its reference counter to 1.
     * @param target An object to add.
     * @return An ID of the stored object.
     * @see globals#free(long)
     */
    static long alloc(Object target) {
        synchronized (storage) {
            long id = ++counter;
            Item item = new Item(target);
            storage.put(id, item);
            return id;
        }
    }

    /**
     * Removes all the global items from the storage.
     */
    static void clear() {
        synchronized (storage) {
            for (long id: storage.keySet()) {
                Item item = storage.get(id);
                item.clear();
            }
            storage.clear();
        }
    }

    /**
     * Removes a stored object by its ID despite the value of its reference counter.
     * @param id ID of an object to remove.
     * @return The removed object, or {@code null} if the specified ID not found.
     * @see globals#alloc(Object)
     */
    static Object free(long id) {
        synchronized (storage) {
            Item item = storage.remove(id);
            if (item == null)
                return null;
            Object target = item.target;
            item.clear();
            return target;
        }
    }

    /**
     * Gets a stored object by its ID. The object's reference counter is not updated.
     * @param id ID of an object to retrieve.
     * @return A stored object that corresponds to the specified ID,
     * or {@code null} if no object with the specified ID exists in the storage.
     * @see globals#set(long, Object)
     */
    static Object get(long id) {
        synchronized (storage) {
            Item item = storage.get(id);
            return (item == null) ? null : item.target;
        }
    }

    /**
     * Decreases the reference counter of an object by its ID in the storage, and removes the object
     * from the storage if its reference counter become 0.
     * @param id An object ID to release.
     * @return The removed object if its reference counter became 0, or {@code null} if either no object
     * with the specified ID exists in the storage or the reference counter for the object is still greater than 0.
     * @see globals#acquire(long)
     */
    static Object release(long id) {
        synchronized (storage) {
            Item item = storage.get(id);
            if (item == null)
                return null;
            if (--item.refs > 0)
                return null;
            storage.remove(id);
            Object target = item.target;
            item.clear();
            return target;
        }
    }

    /**
     * Stores a new object by existing ID. The object's reference counter is not updated.
     * @param id     ID of an object to replace.
     * @param target A new object to store.
     * @return A previously stored object, or {@code null} if either the specified ID not found or
     * the {@code target} is already stored with the specified ID.
     */
    static Object set(long id, Object target) {
        synchronized (storage) {
            Item item = storage.get(id);
            if (item == null)
                return null;
            if (item.target == target)
                return null;
            Object old = item.target;
            item.target = target;
            return old;
        }
    }

    private static class Item {
        int refs;
        Object target;

        Item(Object target) {
            super();
            this.refs = 1;
            this.target = target;
        }

        void clear() {
            refs = 0;
            target = null;
        }
    }
}
