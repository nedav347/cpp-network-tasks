# Repository with a code for the Geekbrains students

Это репозиторий, содержащий код задания для студентов, проходящих курс "Сетевое программирование на C++" в GeekBrains.


## Начало работы

Чтобы начать работать:

- Склонируйте репозиторий: `git clone https://github.com/artiomn/cpp-network-tasks.git`
- Зайдите в каталог `cpp-network-tasks`.
- Запустите скрипт `./build_dockerized.sh`

Если это первый запуск, пройдёт значительно время перед тем, как вы получите результат.


## FAQ

### Как сделать домашнее задание, чтобы преподаватель его посмотрел?

Чтобы ваше задание не получило статус **"Не сдано"** автоматически, ещё до проверки его преподавателем, выполните несколько правил:

- Проверьте, что код скомпилируется.
- Оформите код согласно кодовому стилю.

Пример оформления вы увидите [в примерах к уроку](src/l1).

Кратко:

- В качестве отступов используются пробелы.
- Ширина отступа **4 пробела**.
- Открывающая фигурная скобка на следующей строки после имени.
- После `if`, `for` и подобных операторов - один пробел до скобки.
- После имён функций и методов нет пробелов.
- После открывающей и перед закрывающей круглыми скобками нет пробелов.
- После каждой запятой и точки с запятой - пробел.
- Названия классов пишутся в `CamelCase`.
- Названия функций, методов и переменных - в `snake_case`.
- Между реализациями функция и методов ставится две пустые строки.
- Внутри тела функций и методов **не допускается** более одной пустой строки.
- Атрибуты классов оканчиваются на `_`.
- Комментарии желательно оставлять на английском языке.
  В примерах они, местами, на русском. Исключительно потому, что примеры учебные.

Правильный вариант оформления:

```cpp
int main()
{
    std::cout
        << "Getting name for \"" << host_name << "\"...\n"
        << "Using getaddrinfo() function." << std::endl;

    addrinfo hints =
    {
        .ai_flags= AI_CANONNAME,
        // Неважно, IPv4 или IPv6.
        .ai_family = AF_UNSPEC,
        // TCP stream-sockets.
        .ai_socktype = SOCK_STREAM,
        // Any protocol.
        .ai_protocol = 0
    };

    // Results.
    addrinfo *servinfo = nullptr;
    int status = 0;

    if ((status = getaddrinfo(host_name.c_str(), nullptr, &hints, &servinfo)) != 0)
    {
        std::cerr << "getaddrinfo error: " << gai_strerror(status) << std::endl;
        return EXIT_FAILURE;
    }

    for (auto const *s = servinfo; s != nullptr; s = s->ai_next)
    {
        std::cout << "Canonical name: ";
        if (s->ai_canonname)
             std::cout << s->ai_canonname;
        std::cout << "\n";

        assert(s->ai_family == s->ai_addr->sa_family);
        std::cout << "Address type: ";

        if (AF_INET == s->ai_family)
        {
            char ip[INET_ADDRSTRLEN];
            std::cout << "AF_INET\n";
            sockaddr_in const * const sin = reinterpret_cast<const sockaddr_in* const>(s->ai_addr);
            std::cout << "Address length: " << sizeof(sin->sin_addr) << "\n";
            in_addr addr = { .s_addr = *reinterpret_cast<const in_addr_t*>(&sin->sin_addr) };

            std::cout << "IP Address: " << inet_ntop(AF_INET, &addr, ip, INET_ADDRSTRLEN) << "\n";
        }
        else if (AF_INET6 == s->ai_family)
        {
            char ip6[INET6_ADDRSTRLEN];

            std::cout << "AF_INET6\n";
        }
        else
        {
            std::cout << s->ai_family << "\n";
        }
        std::cout << std::endl;
    }
    std::cout << std::endl;

    freeaddrinfo(servinfo);

    return EXIT_SUCCESS;
}
```


### Я художник, я так вижу, мне плевать на правила оформления

Ваша работа автоматически отправляется в помойку.


### Почему используется Linux?

- Потому что, на Linux и BSD системах работают большинство сетевых приложений.
- На Linux работает автор курсов.
- Кроме Linux есть множество других ОС, и рассмотреть особенности каждой - невозможно.
- Перевод кода на Windows оговорён.
- По возможности, код и так кроссплатформенный.


### Где все примеры?

Те, что сделаны, - в надёжном месте. Они будут добавляться по мере прохождения уроков.
Доступные вам примеры находятся в каталоге `src/l<номер_урока>`.


### Как собрать примеры?

Сборка производится, используя CMake, поэтому:

- Вы можете использовать любую современную IDE для сборки.
- Скрипт `./build.sh`.
- Прямой запуск CMake.

Проблемой является то, что сборка не подгружает зависимости.
Поэтому, для того, чтобы собрать без необходимости заниматься установкой множества пакетов, используется подготовленная среда.
Она представляет собой Docker-контейнер, образ которого лежит на docker.hub.
Сборка в ней запускается через скрипт `./build_dockerized.sh`.


### Как запустить собранное?

Если вы производили сборку, как указано выше, собранные бинарные файлы будут находиться в каталоге `build/bin`.
Запустить вы их можете напрямую, однако из-за отсутствующих в системе зависимостей работать может не всё.
Поэтому, запуск также производится в контейнере, используя команду `./run`, которой передаются необходимые приложению аргументы и путь к нему.

Пример:

```
➭ ./run sudo ./build/bin/ping google.com
Pinging "google.com" [64.233.165.113]
Start to sending packets...
Sending packet 0 to "google.com" request with id = 29
Receiving packet 0 from "google.com" response with id = 29, time = 15.5ms
```


### Как запустить консоль?

- `./run`
- `docker-compose run --rm gb` - для тех, кто пользуется docker-compose.
