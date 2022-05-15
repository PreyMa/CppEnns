namespace Stream {

    namespace Detail {

        struct Exception {};

        template<typename T> struct removeReference      { using type = T; };
        template<typename T> struct removeReference<T&>  { using type = T; };
        template<typename T> struct removeReference<T&&> { using type = T; };

        template<typename T>
        using removeReferenceType = typename removeReference<T>::type;

        template<typename T>
        constexpr removeReferenceType<T>&& move(T&& x) noexcept {
            return static_cast<removeReferenceType<T>&&>(x);
        }

        template<typename T>
        class TypedStorage {
            union Storage {
                char space[sizeof(T)];
                T value;

                Storage() : space{} {}
                Storage(const T& x) : value{ x } {}
                Storage(T&& x) : value{ move(x) } {}
            } data;

        public:
            TypedStorage()
                : data{} {}

            TypedStorage(const T& x)
                : data{ x } {}

            TypedStorage(T&& x)
                : data{ move(x) } {}

            void construct(const T& x) {
                new(&data.value) T(x);
            }

            void construct(T&& x) {
                new(&data.value) T(move(x));
            }

            void destruct() {
                data.value.~T();
            }

            T& get() {
                return data.value;
            }

            const T& get() const {
                return data.value;
            }
        };
    }

    template<typename TIt>
    class EmptyStream;

    template<typename TIt>
    class VectorStream;

    template<typename TIt, typename TLam>
    class MapStream;

    template<typename TIt, typename TLam>
    class FlatMapStream;

    template<typename TIt, typename TLam>
    class FilterStream;

    template<typename TIt, typename TLam>
    class TapStream;

    template<typename TIt>
    class LimitStream;

    template<typename TContainer>
    auto of(TContainer& container) {
        return VectorStream<typename TContainer::iterator>{ container.begin(), container.end() };
    }

    template<typename T>
    auto of(T* begin, T* end) {
        return VectorStream<T*>{ begin, end };
    }

    template<typename T>
    auto of(const T* begin, const T* end) {
        return VectorStream<const T*>{ begin, end };
    }

    template<typename T, std::size_t size>
    auto of(T(&data)[size]) {
        return VectorStream<T*>{ data, data + size };
    }

    template<typename T, std::size_t size>
    auto of(const T(&data)[size]) {
        return VectorStream<const T*>{ data, data + size };
    }

    template<typename T>
    auto empty() {
        return EmptyStream<T>{};
    }


    template<typename TIt>
    class Stream {
    protected:
        using TIterator = TIt;

        TIterator iterator;

    public:
        Stream(TIterator it)
            : iterator{ Detail::move(it) } {}

        auto& getIterator() {
            return iterator;
        }

        template<typename TLam>
        auto map(TLam lambda) {
            return MapStream<TIterator, TLam>{ iterator, lambda };
        }

        template<typename TLam>
        auto flatMap(TLam lambda) {
            return FlatMapStream<TIterator, TLam>{ iterator, lambda };
        }

        template<typename TLam>
        auto filter(TLam lambda) {
            return FilterStream<TIterator, TLam>{ iterator, lambda };
        }

        template<typename TLam>
        auto tap(TLam lambda) {
            return TapStream<TIterator, TLam>{ iterator, lambda };
        }

        auto limit(int size) {
            return LimitStream<TIterator>{ iterator, size };
        }

        void sink() {
            while (iterator.hasNext()) {
                iterator.next();
            }
        }

        template<typename TFunc>
        void forEach(TFunc f) {
            while (iterator.hasNext()) {
                f(iterator.next());
            }
        }

        template<typename TFunc, typename TValue>
        TValue reduce(TFunc f, TValue accu) {
            while (iterator.hasNext()) {
                accu = f(iterator.next(), accu);
            }

            return accu;
        }

        template<typename TFunc>
        bool allMatch(TFunc f) {
            while (iterator.hasNext()) {
                if (!f(iterator.next())) {
                    return false;
                }
            }

            return true;
        }

        template<typename TFunc>
        bool anyMatch(TFunc f) {
            while (iterator.hasNext()) {
                if (f(iterator.next())) {
                    return true;
                }
            }

            return false;
        }

        /*template<typename TFunc>
        auto firstMatch(TFunc f) {
            while (iterator.hasNext()) {
                auto x= iterator.next();
                if (f(x)) {
                    return Optional::of(x);
                }
            }

            return Optional::empty();
        }*/

        int count() {
            int ctr = 0;
            while (iterator.hasNext()) {
                iterator.next();
                ctr++;
            }

            return ctr;
        }

        template<typename TFunc>
        int count(TFunc f) {
            int ctr = 0;
            while (iterator.hasNext()) {
                if (f(iterator.next())) {
                    ctr++;
                }
            }

            return ctr;
        }

        template<typename TContainer>
        void emplaceInto(TContainer& cont) {
            while (iterator.hasNext()) {
                cont.emplace_back(iterator.next());
            }
        }
    };


    template<typename TIt>
    class StreamIterator {
        TIt current;
        TIt end;

    public:
        StreamIterator(TIt b, TIt e)
            : current{b}, end{e} {}

        bool hasNext() {
            return current != end;
        }

        int estimateRemaining() {
            return end - current;
        }

        auto next() {
            auto x = Detail::move(*current);
            current++;
            return x;
        }
    };

    template<typename TIt>
    class VectorStream : public Stream<StreamIterator<TIt>> {
    public:
        VectorStream(TIt b, TIt e)
            : Stream<StreamIterator<TIt>>{ StreamIterator<TIt>{Detail::move(b), Detail::move(e)} } {}
    };

    template<typename T>
    class EmptyIterator {

    public:

        bool hasNext() {
            return false;
        }

        int estimateRemaining() {
            return 0;
        }

        T next() {
            throw Detail::Exception{};
        }
    };

    template<typename T>
    class EmptyStream : public Stream<EmptyIterator<T>> {
    public:
        EmptyStream()
            : Stream<EmptyIterator<T>>{ EmptyIterator<T>{} } {}
    };


    template<typename TStreamIt, typename TLam>
    class MapIterator {
        TStreamIt it;
        TLam lambda;

    public:
        MapIterator(TStreamIt i, TLam l)
            : it{i}, lambda{l} {}

        bool hasNext() {
            return it.hasNext();
        }

        int estimateRemaining() {
            return it.estimateRemaining();
        }

        auto next() {
            return lambda(it.next());
        }
    };

    template<typename TStreamIt, typename TLam>
    class MapStream : public Stream<MapIterator<TStreamIt, TLam>> {
    public:
        MapStream(TStreamIt i, TLam l)
            : Stream<MapIterator<TStreamIt, TLam>>{ MapIterator<TStreamIt, TLam>{Detail::move(i), Detail::move(l)} } {}
    };


    template<typename TStreamIt, typename TLam>
    class FlatMapIterator {
        TStreamIt streamIt;
        TLam lambda;

        using TInnerIt= Detail::removeReferenceType<decltype(lambda(streamIt.next()).getIterator())>;
        Detail::TypedStorage<TInnerIt> innerIt;
        bool hasValue{ false };
        bool hasInnerIt{ false };

        void moveNext() {
            hasValue = false;

            if (hasInnerIt && innerIt.get().hasNext()) {
                hasValue = true;
                return;
            }

            while (streamIt.hasNext()) {
                if (hasInnerIt) {
                    innerIt.get()= Detail::move(lambda(streamIt.next()).getIterator());

                }
                else {
                    innerIt.construct( Detail::move(lambda(streamIt.next()).getIterator()) );
                    hasInnerIt = true;
                }

                if (innerIt.get().hasNext()) {
                    hasValue = true;
                    return;
                }
            }
        }

    public:
        FlatMapIterator(TStreamIt i, TLam l)
            : streamIt{ i }, lambda{ l } {
            moveNext();
        }

        FlatMapIterator(const FlatMapIterator& x)
            : streamIt{ x.streamIt }, lambda{ x.lambda }, hasValue{ x.hasValue }, hasInnerIt{ x.hasInnerIt } {
            if (x.hasInnerIt) {
                innerIt.construct(x.innerIt.get());
            }
        }

        FlatMapIterator(FlatMapIterator&& x)
            : streamIt{ Detail::move(x.streamIt) }, lambda{ Detail::move(x.lambda) }, hasValue{ x.hasValue }, hasInnerIt{ x.hasInnerIt } {
            if (x.hasInnerIt) {
                innerIt.construct(Detail::move(x.innerIt.get()));
            }
        }

        ~FlatMapIterator() {
            if (hasInnerIt) {
                innerIt.destruct();
            }
        }

        bool hasNext() {
            return hasValue;
        }

        int estimateRemaining() {
            return streamIt.estimateRemaining();
        }

        auto next() {
            auto x= innerIt.get().next();
            moveNext();

            return x;
        }
    };

    template<typename TStreamIt, typename TLam>
    class FlatMapStream : public Stream<FlatMapIterator<TStreamIt, TLam>> {
    public:
        FlatMapStream(TStreamIt i, TLam l)
            : Stream<FlatMapIterator<TStreamIt, TLam>>{ FlatMapIterator<TStreamIt, TLam>{Detail::move(i), Detail::move(l)} } {}
    };


    template<typename TStreamIt, typename TLam>
    class TapIterator {
        TStreamIt it;
        TLam lambda;

    public:
        TapIterator(TStreamIt i, TLam l)
            : it{ i }, lambda{ l } {}

        bool hasNext() {
            return it.hasNext();
        }

        int estimateRemaining() {
            return it.estimateRemaining();
        }

        auto next() {
            auto x = it.next();
            lambda(x);
            return x;
        }
    };


    template<typename TStreamIt, typename TLam>
    class TapStream : public Stream<TapIterator<TStreamIt, TLam>> {
    public:
        TapStream(TStreamIt i, TLam l)
            : Stream<TapIterator<TStreamIt, TLam>>{ TapIterator<TStreamIt, TLam>{Detail::move(i), Detail::move(l)} } {}
    };

    template<typename TStreamIt>
    class LimitIterator {
        TStreamIt it;
        int size;
        int idx;

    public:
        LimitIterator(TStreamIt i, int s)
            : it{ i }, size{ s }, idx{ 0 } {}

        bool hasNext() {
            return  idx < size&& it.hasNext();
        }

        int estimateRemaining() {
            int rem = it.estimateRemaining();
            return rem < (size - idx) ? rem : (size - idx);
        }

        auto next() {
            idx++;
            return it.next();
        }
    };

    template<typename TStreamIt>
    class LimitStream : public Stream<LimitIterator<TStreamIt>> {
    public:
        LimitStream(TStreamIt i, int s)
            : Stream<LimitIterator<TStreamIt>>{ LimitIterator<TStreamIt>{Detail::move(i), s} } {}
    };


    template<typename TStreamIt, typename TLam>
    class FilterIterator {
        TStreamIt it;
        TLam lambda;

        using TValue = decltype(it.next());

        bool hasValue{ false };
        bool hasInit{ false };
        Detail::TypedStorage<TValue> currentValue;

        void moveNext() {
            hasValue = false;
            while (it.hasNext()) {
                auto x = Detail::move( it.next() );
                if (lambda(x)) {
                    if (!hasInit) {
                        currentValue.construct(Detail::move(x));
                        hasInit = true;
                    }
                    else {
                        currentValue.get() = x;
                    }
                    hasValue = true;
                    return;
                }
            }
        }

    public:
        FilterIterator(TStreamIt i, TLam l)
            : it(i), lambda(l) {
            moveNext();
        }

        FilterIterator(const FilterIterator& x)
            : it(x.it), lambda(x.lambda), hasValue(x.hasValue), hasInit(x.hasInit) {
            if (hasInit) {
                currentValue.construct(x.currentValue.get());
            }
        }

        FilterIterator(FilterIterator&& x)
            : it(Detail::move(x.it)), lambda(Detail::move(x.lambda)), hasValue(x.hasValue), hasInit(x.hasInit) {
            if (hasInit) {
                currentValue.construct(Detail::move(x.currentValue.get()));
            }
        }

        ~FilterIterator() {
            if (hasInit) {
                currentValue.destruct();
            }
        }

        int estimateRemaining() {
            return it.estimateRemaining();
        }

        bool hasNext() {
            return hasValue;
        }

        auto next() {
            auto x = currentValue.get();
            moveNext();
            return x;
        }
    };

    template<typename TStreamIt, typename TLam>
    class FilterStream : public Stream<FilterIterator<TStreamIt, TLam>> {
    public:
        FilterStream(TStreamIt i, TLam l)
            : Stream<FilterIterator<TStreamIt, TLam>>{ FilterIterator<TStreamIt, TLam>{Detail::move(i), Detail::move(l)} } {}
    };
}