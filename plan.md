# Chatter

## Plan

-   There will be one server only
-   All clients will connect to the above server
-   Server will handle grouping and messages between groups
-   GUI for client side
-   File sending

## Dunno if possible

-   Voice chat

## Things not planned

-   One user, many rooms
-   GUI for server side

## Details

-   **`group`**: a chat room

    -   `group_id`: ID used for inviting
    -   `password`: room password
    -   `clients[]`: room members

-   **`client`**: chatter user

    -   socket stuff
    -   `name`: username
    -   `group_id`: room

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

## How things should work

-   An user will only send message to users of the same room
