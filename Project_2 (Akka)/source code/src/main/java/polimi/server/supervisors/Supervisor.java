package polimi.server.supervisors;

import akka.actor.SupervisorStrategy;

public interface Supervisor {

    SupervisorStrategy supervisorStrategy();
}
