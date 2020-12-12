# Chatter

## Progress

- [x] Chat function
- [x] Create room
- [x] Join room
- [x] Switch room

## Plan

-   There will be one server only
-   All clients will connect to the above server
-   Server will handle grouping and messages between groups
-   One client can join many servers
-   GUI for client side
-   File sending

## Dunno if possible

-   Voice chat

## Things not planned

-   GUI for server side

## Details

-   **`group`**: a chat room

    -   `group_id`: ID used for inviting
    -   `password`: room password
    -   `clients[]`: room members

-   **`client`**: chatter user

    -   socket stuff
    -   `name`: username
    -   `groups`: joined rooms
    -   `active_group`: focused room

-   **`server`**: the one behind all
    -   socket stuff
    -   `groups[]`: all rooms
    -   `clients[]`: all clients

## UI Flows

-   When an user open the application, there will be a choice to select **Join** or **Create**

-   If user chooses **Join**

    -   Group ID
    -   Password
    -   Username

-   If **Create**

    -   Group name
    -   Password
    -   Username

-   Chat UI
    -   Group name, group ID
    -   Messages
        -   Name
        -   Time
        -   Message content
    -   Input box
    -   File attachment button

## Console

-   `:c <password>`: Create room
-   `:j <id> <password>`: Join room with password
-   `:s <id>`: Switch focus to room
-   `:h`: Print help
-   `:i`: Print room info
-   `:u`: Print roommates info
-   `:f`: Send file
-   `<other>`: Send message

## How things should work

-   User will only send message to users of the same room
-   User can create, join or switch between rooms
