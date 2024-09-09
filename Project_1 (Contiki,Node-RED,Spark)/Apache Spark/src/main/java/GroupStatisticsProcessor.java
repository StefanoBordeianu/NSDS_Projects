import org.apache.spark.SparkConf;
import org.apache.spark.api.java.JavaSparkContext;
import org.apache.spark.sql.Dataset;
import org.apache.spark.sql.Row;
import org.apache.spark.sql.SparkSession;
import org.apache.spark.sql.streaming.StreamingQuery;
import org.apache.spark.sql.streaming.StreamingQueryException;
import org.apache.spark.sql.types.DataTypes;
import org.apache.spark.sql.types.Metadata;
import org.apache.spark.sql.types.StructField;
import org.apache.spark.sql.types.StructType;

import java.util.concurrent.TimeoutException;

import static org.apache.spark.sql.functions.*;

public class GroupStatisticsProcessor {
    public static void main(String[] args) throws StreamingQueryException, TimeoutException {

        final String master = args.length > 0 ? args[0] : "local[4]";

        SparkConf conf = new SparkConf()
                .setAppName("GroupStatistics")
                .setMaster(master)
                .set("spark.driver.bindAddress", "localhost");

        JavaSparkContext sc = new JavaSparkContext(conf);

        SparkSession spark = SparkSession.builder()
                .config(conf)
                .config("spark.jars.packages", "org.apache.bahir.spark-sql-streaming-mqtt_2.11:2.4.0")
                .getOrCreate();

        // Define schema for input data
        StructType schema = new StructType(new StructField[]{
                new StructField("ip", DataTypes.IntegerType, false, Metadata.empty()),
                new StructField("timestamp", DataTypes.TimestampType, false, Metadata.empty()),
                new StructField("nationality", DataTypes.StringType, false, Metadata.empty()),
                new StructField("age", DataTypes.IntegerType, false, Metadata.empty())
        });

        spark.conf().set("spark.sql.streaming.statefulOperator.checkCorrectness.enabled", false);

        // Read streaming data from MQTT
        Dataset<Row> mqttStream = spark
                .readStream()
                .format("org.apache.bahir.sql.streaming.mqtt.MQTTStreamSourceProvider")
                .option("topic", "nsds/prova")
                .load("broker.hivemq.com:1883")
                .selectExpr("CAST(value AS STRING) as json")
                .select(from_json(col("json"), schema).as("data"))
                .select("data.*");

        // Add watermark to handle late data
        Dataset<Row> events = mqttStream
                .withWatermark("timestamp", "1 day");

        // Calculate daily average age per nationality
        Dataset<Row> dailyAvgAge = events
                .groupBy(window(col("timestamp"), "1 day"), col("nationality"))
                .agg(avg("age").as("daily_avg_age"))
                .select(col("window.end").as("date"), col("nationality"), col("daily_avg_age"));

        // Calculate 7-day moving average and percentage increase
        Dataset<Row> movingAvgAndIncrease = dailyAvgAge
                .withWatermark("date", "7 days")
                .groupBy(col("nationality"),
                        window(col("date"), "7 days", "1 day"))
                .agg(avg("daily_avg_age").as("moving_avg_age"),
                        first("daily_avg_age").as("first_day_avg"),
                        last("daily_avg_age").as("last_day_avg"))
                .select(col("window.end").as("date"),
                        col("nationality"),
                        col("moving_avg_age"),
                        ((col("last_day_avg").minus(col("first_day_avg"))).divide(col("first_day_avg"))).multiply(100).as("percent_increase"));

        // Find top nationalities with highest percentage increase
        Dataset<Row> topNationalities = movingAvgAndIncrease
                .groupBy("date")
                .agg(max(struct(col("percent_increase"), col("nationality"))).alias("top"))
                .select(col("date"),
                        col("top.nationality").as("top_nationality"),
                        col("top.percent_increase").as("highest_increase"));

        // Output results
        StreamingQuery movingAvgQuery = movingAvgAndIncrease
                .writeStream()
                .outputMode("append")
                .format("console")
                .option("truncate", false)
                .start();

        StreamingQuery topNationalitiesQuery = topNationalities
                .writeStream()
                .outputMode("complete")
                .format("console")
                .option("truncate", false)
                .start();

        // Wait for queries to terminate
        spark.streams().awaitAnyTermination();
    }
}