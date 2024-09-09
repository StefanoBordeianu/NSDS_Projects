package polimi.server.messages;

import akka.actor.ActorRef;
import lombok.Getter;
import lombok.Setter;

import java.util.Map;

@Getter
@Setter
public class AskForActorMap {

    private Map<String, ActorRef> actorMap;

}
