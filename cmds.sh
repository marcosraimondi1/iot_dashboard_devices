# first get device credentials
curl \
	-H "Content-type: application/x-www-form-urlencoded" \
	-d "dId=1&password=GAGX2hgDZ9" \
	-X POST localhost:3001/api/getdevicecredentials

# credentials change after certain delay, so you need to connect fast

# response example
{
	"username":"fgqOt6H5sD",
	"password":"8yt30cscDt",
	"topic":"679103d95d9261fdf4d81397/1/",
	"variables":[
	{
		"variable":"goRrYVqZw2",
		"variableFullName":"LED Status",
		"variableType":"input",
		"variableSendFreq":30
	},
	{
		"variable":"6YYrHcvKaK",
		"variableFullName":"LED Control",
		"variableType":"output"
	}]
}

# with mosquitto clients
# - subscribe
mosquitto_sub --url mqtt://username:password@host:port/topic
# example: subscribe to all actuators from device
mosquitto_sub --url mqtt://HuGFf0WiYR:n8ayKSgIq4@localhost:1883/67923ede494488f95645b3aa/1/+/actdata

# - publish
mosquitto_pub --url mqtt://username:password@host:port/topic -m 'message'
# example: send msg from certain sensor variable
mosquitto_pub --url mqtt://HuGFf0WiYR:n8ayKSgIq4@localhost:1883/67923ede494488f95645b3aa/1/hJo2gk3hfX/sdata -m '{"value": true}'

