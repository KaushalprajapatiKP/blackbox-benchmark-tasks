# qsrv user guide

qsrv is a small in-memory broker that offers job queues and a versioned key/value
store over a plain TCP text protocol. It is meant for tests and local tooling: all
state lives in memory and time inside the server is a logical clock that only moves
when a client asks it to.

## Running

    qsrv --port 7070

The server listens on 127.0.0.1. Any number of clients may connect at once, up to a
fixed limit.

## Talking to the server

Clients send one command per line. A command is a name followed by arguments
separated by spaces. Command names are not case sensitive. Arguments that contain
spaces or special bytes can be quoted with double quotes (with backslash escapes) or
single quotes (taken literally).

Each command produces one reply. Replies start with a type marker:

| Marker | Meaning                               |
|--------|---------------------------------------|
| `+`    | status text                           |
| `-`    | error: an error code, then a message  |
| `:`    | integer                               |
| `$`    | byte string: length line, then data   |
| `*`    | array: element count, then elements   |

Every reply line ends with CR LF. Clients may send several commands without waiting
for replies; replies come back in order.

## Logical time

The server clock starts at 0. `TICK n` moves it forward by n ticks. Delays, job
visibility timeouts, key expiry and blocking waits are all measured in ticks.

## Namespaces

Each connection works inside a namespace, `default` at first. Queues, jobs and keys
in one namespace are invisible from another.

## Commands

### Connection

| Command          | Purpose                                         |
|------------------|-------------------------------------------------|
| `PING [msg]`     | liveness check                                  |
| `ECHO msg`       | returns msg                                     |
| `USE ns`         | switch this connection to namespace ns          |
| `WHOAMI`         | shows this connection's id and namespace        |
| `QUIT`           | closes the connection                           |

### Clock

| Command   | Purpose                         |
|-----------|---------------------------------|
| `TICK n`  | advance the clock by n ticks    |
| `TIME`    | current clock value             |

### Queues

| Command                                   | Purpose                                              |
|-------------------------------------------|------------------------------------------------------|
| `QDECLARE q [MAXLEN n] [MAXNACK n]`       | create a queue with optional limits                  |
| `QDROP q`                                 | delete a queue and its jobs                          |
| `QLIST`                                   | list queues in the namespace                         |
| `PUSH q payload [PRI p] [DELAY d] [TTR t]`| add a job; returns its id                            |
| `RESERVE q`                               | take the next job, if any                            |
| `BRESERVE q timeout`                      | like RESERVE but waits for a job                     |
| `ACK id`                                  | finish a reserved job                                |
| `NACK id [DELAY d]`                       | give a job back as failed                            |
| `RELEASE id [DELAY d] [PRI p]`            | give a job back without counting a failure           |
| `TOUCH id`                                | extend the reservation of a job                      |
| `PEEK q`                                  | look at the next job without taking it               |
| `QSTAT q`                                 | queue statistics                                     |
| `DEADLIST q [count]`                      | list dead-lettered jobs                              |
| `KICK q [count]`                          | move dead-lettered jobs back into the queue          |
| `JOBINFO id`                              | details of one job                                   |

A reserved job must be acknowledged before its time-to-run (TTR) runs out, otherwise
it goes back to the queue. Jobs that fail too often are dead-lettered.

### Keys

| Command                               | Purpose                                     |
|---------------------------------------|---------------------------------------------|
| `SET key value [TTL t] [NX] [XX]`     | store a value; returns the new version      |
| `GET key`                             | read a value                                |
| `GETV key`                            | read a value together with its version      |
| `CAS key version value`               | write only if the version matches           |
| `DEL key [key ...]`                   | delete keys                                 |
| `INCR key [delta]`                    | add to an integer value                     |
| `KEYS [pattern]`                      | list keys, optionally filtered by a glob    |
| `TTL key`                             | remaining lifetime of a key                 |
| `EXPIRE key t`                        | set the lifetime of a key                   |

### Transactions

| Command    | Purpose                               |
|------------|---------------------------------------|
| `MULTI`    | start queueing commands               |
| `EXEC`     | run the queued commands               |
| `DISCARD`  | drop the queued commands              |

### Server

| Command            | Purpose                                         |
|--------------------|-------------------------------------------------|
| `INFO [section]`   | server information                              |
| `FLUSH`            | remove everything in the current namespace      |

## Examples

See `examples/` for a few short transcripts.
