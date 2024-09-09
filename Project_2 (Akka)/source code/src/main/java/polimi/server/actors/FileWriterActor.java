package polimi.server.actors;

import akka.actor.AbstractActor;
import akka.actor.Props;
import akka.actor.ReceiveTimeout;
import com.fasterxml.jackson.databind.ObjectMapper;
import com.fasterxml.jackson.databind.SerializationFeature;
import polimi.server.messages.AggregatedData;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;
import scala.concurrent.duration.Duration;

import java.io.*;
import java.nio.file.*;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;
import java.util.concurrent.TimeUnit;

public class FileWriterActor extends AbstractActor {
    private static final Logger log = LoggerFactory.getLogger(FileWriterActor.class);
    private static final int BATCH_SIZE = 100;
    private static final int MAX_RETRIES = 3;
    private static final long RETRY_DELAY_MS = 1000;
    private static final long FLUSH_INTERVAL_MS = 3000; // 3 seconds

    private final String filePath;
    private final ObjectMapper objectMapper;
    private final List<AggregatedData> dataToWrite;
    private long lastFlushTime;
    private boolean isFirstWrite;

    public FileWriterActor(String filePath) {
        this.filePath = filePath;
        this.objectMapper = new ObjectMapper();
        this.objectMapper.enable(SerializationFeature.INDENT_OUTPUT);
        this.dataToWrite = new ArrayList<>();
        this.lastFlushTime = System.currentTimeMillis();
        this.isFirstWrite = !Files.exists(Paths.get(filePath));

        getContext().setReceiveTimeout(Duration.create(30, TimeUnit.SECONDS));
    }

    public static Props props(String filePath) {
        return Props.create(FileWriterActor.class, () -> new FileWriterActor(filePath));
    }

    @Override
    public Receive createReceive() {
        return receiveBuilder()
                .match(AggregatedData.class, this::addToWriteQueue)
                .match(ReceiveTimeout.class, msg -> stop())
                .matchAny(msg -> log.warn("Received unexpected message: {}", msg))
                .build();
    }

    private void addToWriteQueue(AggregatedData aggregatedData) {
        dataToWrite.add(aggregatedData);
        if (dataToWrite.size() >= BATCH_SIZE || shouldFlush()) {
            writeQueuedData();
        }
    }

    private boolean shouldFlush() {
        long currentTime = System.currentTimeMillis();
        return currentTime - lastFlushTime >= FLUSH_INTERVAL_MS;
    }

    private void stop() {
        writeQueuedData();
        finalizeJsonArray();
        getContext().stop(self());
    }

    private void writeQueuedData() {
        if (dataToWrite.isEmpty()) {
            return;
        }

        for (int attempt = 0; attempt < MAX_RETRIES; attempt++) {
            try {
                appendJsonToFile();
                dataToWrite.clear();
                lastFlushTime = System.currentTimeMillis();
                return;
            } catch (IOException e) {
                log.error("Failed to write aggregated data to file (attempt {}): ", attempt + 1, e);
                if (attempt < MAX_RETRIES - 1) {
                    try {
                        Thread.sleep(RETRY_DELAY_MS);
                    } catch (InterruptedException ie) {
                        Thread.currentThread().interrupt();
                        log.error("Interrupted while waiting to retry", ie);
                    }
                }
            }
        }
        log.error("Failed to write data after {} attempts", MAX_RETRIES);
    }

    private void appendJsonToFile() throws IOException {
        Path path = Paths.get(filePath);
        boolean fileExists = Files.exists(path);

        try (BufferedWriter writer = new BufferedWriter(new FileWriter(filePath, true))) {
            if (isFirstWrite) {
                writer.write("[\n");
                isFirstWrite = false;
            } else if (fileExists) {
                // Remove the closing bracket if it exists
                removeLastChar(path);
                writer.write(",\n");
            }

            for (int i = 0; i < dataToWrite.size(); i++) {
                String json = objectMapper.writeValueAsString(dataToWrite.get(i));
                writer.write(json);
                if (i < dataToWrite.size() - 1) {
                    writer.write(",\n");
                } else {
                    writer.write("\n");
                }
            }

            writer.write("]");
        }
    }

    private void removeLastChar(Path path) throws IOException {
        byte[] bytes = Files.readAllBytes(path);
        if (bytes.length > 0 && bytes[bytes.length - 1] == ']') {
            Files.write(path, Arrays.copyOf(bytes, bytes.length - 1));
        }
    }

    private void finalizeJsonArray() {
        try {
            Path path = Paths.get(filePath);
            List<String> lines = Files.readAllLines(path);
            if (!lines.isEmpty()) {
                String lastLine = lines.get(lines.size() - 1);
                if (!lastLine.trim().equals("]")) {
                    try (BufferedWriter writer = new BufferedWriter(new FileWriter(filePath, true))) {
                        writer.write("\n]");
                    }
                }
            }
        } catch (IOException e) {
            log.error("Failed to finalize JSON array", e);
        }
    }
}