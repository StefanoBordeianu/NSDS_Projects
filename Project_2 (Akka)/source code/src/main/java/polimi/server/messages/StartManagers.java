package polimi.server.messages;

import com.typesafe.config.Config;
import lombok.Getter;

import java.util.Set;

@Getter
public class StartManagers {

    private final Set<String> keys;

    public StartManagers(Config config) {
        keys = config.getObject("stream-processing.partitions").unwrapped().keySet();
    }
}
